/*
 * btspeaker.c - Bluetooth-Lautsprecher (A2DP-Sink) für KaRadio32 (dansoft-ch/KaRadio)
 *
 * Das Handy verbindet sich mit "Biennophone" und spielt Musik über den
 * I2S-Ausgang von KaRadio (render_samples). Lautstärke und Mute laufen damit
 * über denselben Weg wie beim Radio.
 *
 * Der Bluetooth-Stack wird beim ersten btSpeakerStart() eingeschaltet und bleibt
 * danach geladen. btSpeakerStop() trennt das Handy und macht das Gerät
 * unsichtbar; so muss der Stack nicht wiederholt ab- und aufgebaut werden.
 * Grundlage: ESP-IDF 3.3 Beispiel examples/bluetooth/a2dp_sink.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "app_main.h"
#include "webclient.h"
#include "audio_renderer.h"
#include "btspeaker.h"

#define TAG "btspeaker"

static volatile bool s_active = false;   // Bluetooth-Betrieb gewählt (Taste 6)
static bool s_stackUp = false;           // Stack einmal erfolgreich gestartet
static bool s_connected = false;
static esp_bd_addr_t s_peer;
static volatile uint32_t s_rate = 44100;
static uint8_t *s_buf = NULL;            // Kopie der Daten, render_samples ändert sie (Lautstärke)
static uint32_t s_bufLen = 0;

static bool s_memReleased = false;      // BT-Speicher ans Radio abgegeben

bool btSpeakerActive(void) { return s_active; }
bool btSpeakerAvailable(void) { return !s_memReleased; }

// Gibt den fest reservierten Bluetooth-Speicher (ca. 56 KB intern) frei.
// Danach geht Bluetooth erst nach einem Neustart wieder.
void btSpeakerReleaseMemory(void)
{
	if (s_memReleased || s_stackUp) return;
	if (esp_bt_controller_mem_release(ESP_BT_MODE_BTDM) == ESP_OK) {
		s_memReleased = true;
		printf("btspeaker: Radio-Betrieb, Bluetooth-Speicher freigegeben (intern frei %u)\n",
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	}
}

static void setVisible(bool on)
{
	esp_bt_gap_set_scan_mode(on ? ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE : ESP_BT_SCAN_MODE_NONE);
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
			// verbunden: nicht mehr für andere sichtbar
			esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE);
		} else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
			s_connected = false;
			printf("btspeaker: Handy getrennt\n");
			setVisible(s_active);
		}
		break;
	case ESP_A2D_AUDIO_STATE_EVT:
		if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
			printf("btspeaker: Wiedergabe startet (%u Hz)\n", s_rate);
			if (s_active) renderer_start();
		} else {
			if (s_active) renderer_zero_dma_buffer();
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
	if (!s_active || len == 0) return;
	if (len > s_bufLen) {
		free(s_buf);
		s_buf = heap_caps_malloc(len, MALLOC_CAP_8BIT);
		s_bufLen = s_buf ? len : 0;
		if (!s_buf) return;
	}
	memcpy(s_buf, data, len);
	pcm_format_t fmt = {
		.sample_rate = s_rate,
		.bit_depth = I2S_BITS_PER_SAMPLE_16BIT,
		.num_channels = 2,
		.buffer_format = PCM_INTERLEAVED,
	};
	render_samples((char*)s_buf, len, &fmt);
}

static bool stackStart(void)
{
	esp_err_t err;
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
	return true;

fail:
	printf("btspeaker: Start fehlgeschlagen: %s\n", esp_err_to_name(err));
	return false;
}

void btSpeakerStart(void)
{
	if (s_active) return;
	if (s_memReleased) {
		printf("btspeaker: Bluetooth erst nach Neustart moeglich\n");
		return;
	}
	if ((renderer_get() == NULL) || (get_audio_output_mode() == VS1053)) {
		printf("btspeaker: nur mit I2S-Ausgang (kein VS1053) moeglich\n");
		return;
	}
	s_active = true;           // sperrt ab jetzt das Radio (clientConnect)
	clientSilentDisconnect();  // Radio stoppen
	printf("btspeaker: Bluetooth-Betrieb, freier Speicher %u (intern %u)\n",
		esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

	if (!s_stackUp) {
		if (!stackStart()) { s_active = false; return; }
		s_stackUp = true;
		printf("btspeaker: Stack gestartet, freier Speicher %u (intern %u)\n",
			esp_get_free_heap_size(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	}
	renderer_start();
	setVisible(true);
	printf("btspeaker: sichtbar als \"%s\"\n", BT_DEVICE_NAME);
}

void btSpeakerStop(void)
{
	if (!s_active) return;
	s_active = false;
	if (s_stackUp) {
		setVisible(false);
		if (s_connected) esp_a2d_sink_disconnect(s_peer);
	}
	renderer_zero_dma_buffer();
	renderer_stop();           // das Radio startet ihn beim Abspielen wieder
	printf("btspeaker: Bluetooth aus, zurueck zum Radio\n");
}
