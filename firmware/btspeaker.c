/*
 * btspeaker.c - Bluetooth-Lautsprecher (A2DP-Sink) für KaRadio32 (dansoft-ch/KaRadio)
 *
 * v2: Bluetooth ist immer an und als "Biennophone" sichtbar, auch während das
 * Radio spielt. Startet das Handy die Wiedergabe, stoppt das Radio und der Ton
 * vom Handy kommt über den I2S-Ausgang von KaRadio (render_samples, damit
 * wirken Lautstärke und Mute). Trennt sich das Handy oder pausiert es länger
 * als BT_PAUSE_MS, spielt wieder der letzte Radiosender.
 *
 * Die Umschaltung läuft in btSpeakerPoll() (aus dem stationkeys-Task), nicht in
 * den Bluetooth-Callbacks.
 *
 * I2S: KaRadio installiert den I2S-Treiber nur, solange ein Radio-Decoder läuft,
 * und baut ihn beim Stoppen wieder ab. Für Bluetooth richten wir ihn selbst ein
 * und bauen ihn vor dem Radio-Start wieder ab (sonst startet der Decoder neu).
 *
 * Grundlage: ESP-IDF 3.3 Beispiel examples/bluetooth/a2dp_sink.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "app_main.h"
#include "webclient.h"
#include "audio_player.h"
#include "audio_renderer.h"
#include "btspeaker.h"

#define TAG "btspeaker"
#define BT_PAUSE_MS     15000   // so lange Pause, dann zurück zum Radio
#define DECODER_WAIT_MS 3000    // max. Wartezeit, bis der Radio-Decoder gestoppt ist

extern player_t* player_config;  // aus app_main
extern void playStation(char* id);

static bool s_stackUp = false;
static volatile bool s_active = false;      // Bluetooth-Ton läuft, Radio gesperrt
static volatile bool s_connected = false;
static volatile bool s_wantBt = false;      // Handy spielt (A2DP gestartet)
static volatile TickType_t s_pauseSince = 0; // Zeitpunkt der Pause, 0 = keine
static volatile bool s_i2sReady = false;    // unser I2S-Treiber ist installiert
static SemaphoreHandle_t s_i2sMux = NULL;   // schützt i2s_write gegen Abbau
static esp_bd_addr_t s_peer;
static volatile uint32_t s_rate = 44100;
static uint8_t *s_buf = NULL;               // Kopie der Daten, render_samples ändert sie
static uint32_t s_bufLen = 0;

bool btSpeakerActive(void) { return s_active; }

static uint32_t freeInternal(void)
{
	return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void gapCb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
	switch (event) {
	case ESP_BT_GAP_AUTH_CMPL_EVT:
		if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
			printf("btspeaker: gekoppelt mit %s\n", param->auth_cmpl.device_name);
		else
			printf("btspeaker: Koppeln fehlgeschlagen (%d)\n", param->auth_cmpl.stat);
		break;
	case ESP_BT_GAP_CFM_REQ_EVT:   // Zahlenvergleich: ohne Anzeige immer bestätigen
		esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
		break;
	default:
		break;
	}
}

static void a2dCb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
	switch (event) {
	case ESP_A2D_CONNECTION_STATE_EVT:
		if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
			s_connected = true;
			memcpy(s_peer, param->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
			printf("btspeaker: Handy verbunden\n");
			esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE);
		} else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
			s_connected = false;
			s_wantBt = false;
			printf("btspeaker: Handy getrennt\n");
			esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
		}
		break;
	case ESP_A2D_AUDIO_STATE_EVT:
		if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
			printf("btspeaker: Handy spielt (%u Hz)\n", s_rate);
			s_pauseSince = 0;
			s_wantBt = true;
		} else {
			printf("btspeaker: Handy pausiert\n");
			s_pauseSince = xTaskGetTickCount();
			if (s_pauseSince == 0) s_pauseSince = 1;
		}
		break;
	case ESP_A2D_AUDIO_CFG_EVT:
		if (param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
			char oct0 = param->audio_cfg.mcc.cie.sbc[0];
			if (oct0 & (0x01 << 6)) s_rate = 32000;
			else if (oct0 & (0x01 << 5)) s_rate = 44100;
			else if (oct0 & (0x01 << 4)) s_rate = 48000;
			else s_rate = 16000;
		}
		break;
	default:
		break;
	}
}

// Läuft im A2DP-Task: PCM 16 Bit, Stereo, verschachtelt.
static void a2dDataCb(const uint8_t *data, uint32_t len)
{
	if (!s_active || !s_i2sReady || len == 0) return;
	if (xSemaphoreTake(s_i2sMux, 0) != pdTRUE) return;   // gerade Umschaltung
	if (s_i2sReady) {
		if (len > s_bufLen) {
			free(s_buf);
			s_buf = heap_caps_malloc(len, MALLOC_CAP_8BIT);
			s_bufLen = s_buf ? len : 0;
		}
		if (s_buf) {
			memcpy(s_buf, data, len);
			pcm_format_t fmt = {
				.sample_rate = s_rate,
				.bit_depth = I2S_BITS_PER_SAMPLE_16BIT,
				.num_channels = 2,
				.buffer_format = PCM_INTERLEAVED,
			};
			renderer_start();   // falls KaRadio den Renderer gestoppt hat
			render_samples((char*)s_buf, len, &fmt);
		}
	}
	xSemaphoreGive(s_i2sMux);
}

void btSpeakerInit(void)
{
	esp_err_t err;
	if (s_stackUp) return;
	if ((renderer_get() == NULL) || (get_audio_output_mode() == VS1053)) {
		printf("btspeaker: nur mit I2S-Ausgang (kein VS1053) moeglich\n");
		return;
	}
	s_i2sMux = xSemaphoreCreateMutex();
	uint32_t before = freeInternal();
	esp_bt_controller_mem_release(ESP_BT_MODE_BLE);   // nur klassisches Bluetooth

	esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	if ((err = esp_bt_controller_init(&cfg)) != ESP_OK) goto fail;
	if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) goto fail;
	if ((err = esp_bluedroid_init()) != ESP_OK) goto fail;
	if ((err = esp_bluedroid_enable()) != ESP_OK) goto fail;

	esp_bt_dev_set_device_name(BT_DEVICE_NAME);
	esp_bt_gap_register_callback(gapCb);

	// Koppeln ohne PIN ("Just Works")
	esp_bt_sp_param_t ptype = ESP_BT_SP_IOCAP_MODE;
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
	esp_bt_gap_set_security_param(ptype, &iocap, sizeof(uint8_t));

	esp_a2d_register_callback(a2dCb);
	esp_a2d_sink_register_data_callback(a2dDataCb);
	if ((err = esp_a2d_sink_init()) != ESP_OK) goto fail;

	esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
	s_stackUp = true;
	printf("btspeaker: sichtbar als \"%s\" (intern frei %u, Stack braucht %d)\n",
		BT_DEVICE_NAME, freeInternal(), (int)before - (int)freeInternal());
	return;

fail:
	printf("btspeaker: Start fehlgeschlagen: %s\n", esp_err_to_name(err));
}

static void enterBt(void)
{
	s_active = true;           // sperrt ab jetzt das Radio (clientConnect)
	clientSilentDisconnect();  // Radio stoppen
	// warten, bis der Radio-Decoder seinen I2S-Treiber abgebaut hat
	for (int t = 0; t < DECODER_WAIT_MS; t += 50) {
		if (!player_config || player_config->decoder_status != RUNNING) break;
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
	vTaskDelay(100 / portTICK_PERIOD_MS);
	xSemaphoreTake(s_i2sMux, portMAX_DELAY);
	if (init_i2s()) s_i2sReady = true;
	else printf("btspeaker: I2S konnte nicht eingerichtet werden\n");
	xSemaphoreGive(s_i2sMux);
	renderer_start();
	printf("btspeaker: Bluetooth-Ton an (intern frei %u)\n", freeInternal());
}

static void leaveBt(bool playRadio)
{
	xSemaphoreTake(s_i2sMux, portMAX_DELAY);
	if (s_i2sReady) {
		s_i2sReady = false;
		renderer_zero_dma_buffer();
		renderer_stop();
		i2s_stop(I2S_NUM_0);
		i2s_driver_uninstall(I2S_NUM_0);   // der Radio-Decoder installiert ihn neu
	}
	xSemaphoreGive(s_i2sMux);
	s_active = false;
	printf("btspeaker: zurueck zum Radio (intern frei %u)\n", freeInternal());
	if (playRadio) {
		char s[4] = "255";                  // 255 = aktueller Sender
		playStation(s);
	}
}

void btSpeakerPoll(void)
{
	if (!s_stackUp) return;
	bool pausedLong = (s_pauseSince != 0) &&
		((xTaskGetTickCount() - s_pauseSince) > (BT_PAUSE_MS / portTICK_PERIOD_MS));

	if (!s_active && s_wantBt && s_pauseSince == 0) enterBt();
	else if (s_active && (!s_connected || !s_wantBt || pausedLong)) {
		s_wantBt = false;
		s_pauseSince = 0;
		leaveBt(true);
	}
}

void btSpeakerToRadio(void)
{
	if (!s_active) return;
	s_wantBt = false;
	s_pauseSince = 0;
	if (s_connected) esp_a2d_sink_disconnect(s_peer);   // sonst holt das Handy den Ton zurück
	leaveBt(false);   // Sender spielt der Aufrufer
}
