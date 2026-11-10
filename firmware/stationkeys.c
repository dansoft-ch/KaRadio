/*
 * stationkeys.c - Ergänzung für KaRadio32 (dansoft-ch/KaRadio)
 *
 * Bis zu 6 Taster wählen direkt einen Sender (Taste n -> Sender n),
 * ein Poti stellt die Lautstärke (0..254) ein.
 * Das ersetzt den ESP32-S3, der bisher cli.play/cli.vol per UART geschickt hat.
 *
 * Dazu (Branch firmware-ng-bt, v2):
 * - Bluetooth-Lautsprecher (btspeaker.c) ist immer an. Spielt das Handy,
 *   schaltet btSpeakerPoll() den Ton aufs Handy; eine Sendertaste trennt das
 *   Handy und spielt wieder Radio.
 * - Mute-Poti (Schleifer des Ein/Aus-Knopfs): es zählt die Drehrichtung.
 *   Um MUTE_STEP aufgedreht -> Mute aus, um MUTE_STEP zugedreht -> Mute an.
 *
 * Konfiguration in der Hardware-Partition (gpio_space), z. B. in der .csv:
 *   P_KEY0 .. P_KEY5   GPIO des Tasters für Sender 0..5 (Taster gegen GND,
 *                      interner Pull-up). 255 = nicht benutzt.
 *   P_VOL_POT          GPIO des Poti-Schleifers, nur ADC1 (32..39).
 *                      Poti-Enden an 3,3 V und GND. 255 = nicht benutzt.
 *   P_MUTE_POT         GPIO des Mute-Poti-Schleifers, nur ADC1 (32..39).
 *                      Ohne angeschlossenes Poti braucht der Pin einen Pull-up
 *                      (GPIO 32/33 intern, 34..39 nur extern, z. B. 100 kOhm
 *                      nach 3,3 V). 255 = nicht benutzt.
 * Fehlen die Einträge, ist die jeweilige Funktion abgeschaltet.
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "nvs.h"
#include "gpio.h"
#include "app_main.h"
#include "webserver.h"
#include "webclient.h"
#include "audio_renderer.h"
#include "btspeaker.h"
#include "stationkeys.h"

#define TAG "stationkeys"

extern const char hardware[];   // aus gpio.c
extern const char gpio_space[];
extern void playStation(char* id); // aus webserver.c, steht in keinem Header

#define NB_KEYS        6
#define LOOP_MS        10      // Abfrage-Takt
#define KEY_STABLE     4       // so oft gleich gelesen = entprellt (40 ms)
#define POT_EVERY      8       // Poti alle 8 Takte (80 ms) auswerten
#define POT_SAMPLES    16      // Mittelwert über 16 Messungen
#define ADC_LOW        80      // Totzone unten (ESP32-ADC misst unter ~0,1 V nichts)
#define ADC_HIGH       3900    // Totzone oben (ab ~3,1 V gesättigt)
#define VOL_MAX        254
#define VOL_HYST       2       // Mindeständerung, bevor neu gesetzt wird
#define VOL_FORCE_MS   60000   // so lange nach dem Start gewinnt das Poti immer
#define MUTE_STEP      410     // ca. 10 % von 4095 (5 % war zu empfindlich)

static gpio_num_t keyPin[NB_KEYS];
static gpio_num_t potPin = GPIO_NONE;
static gpio_num_t mutePin = GPIO_NONE;
static adc1_channel_t potChan, muteChan;

static int adcChannel(gpio_num_t gpio)
{
	switch (gpio) {
		case 36: return ADC1_CHANNEL_0;
		case 37: return ADC1_CHANNEL_1;
		case 38: return ADC1_CHANNEL_2;
		case 39: return ADC1_CHANNEL_3;
		case 32: return ADC1_CHANNEL_4;
		case 33: return ADC1_CHANNEL_5;
		case 34: return ADC1_CHANNEL_6;
		case 35: return ADC1_CHANNEL_7;
		default: return -1;
	}
}

static gpio_num_t readPin(nvs_handle handle, const char *key)
{
	uint8_t v;
	if (nvs_get_u8(handle, key, &v) == ESP_OK) return v;
	return GPIO_NONE;
}

static void readConfig(void)
{
	nvs_handle handle;
	char key[8];
	for (int i = 0; i < NB_KEYS; i++) keyPin[i] = GPIO_NONE;
	potPin = mutePin = GPIO_NONE;

	if (open_partition(hardware, gpio_space, NVS_READONLY, &handle) != ESP_OK) return;
	for (int i = 0; i < NB_KEYS; i++)
	{
		sprintf(key, "P_KEY%d", i);
		keyPin[i] = readPin(handle, key);
	}
	potPin = readPin(handle, "P_VOL_POT");
	mutePin = readPin(handle, "P_MUTE_POT");
	close_partition(handle, hardware);
}

static int readAdc(adc1_channel_t chan)
{
	int sum = 0;
	for (int i = 0; i < POT_SAMPLES; i++) sum += adc1_get_raw(chan);
	return sum / POT_SAMPLES;
}

static int readPot(void)
{
	int raw = readAdc(potChan);
	if (raw < ADC_LOW) raw = ADC_LOW;
	if (raw > ADC_HIGH) raw = ADC_HIGH;
	return (raw - ADC_LOW) * VOL_MAX / (ADC_HIGH - ADC_LOW);
}

static void applyVolume(int vol)
{
	char s[5];
	sprintf(s, "%d", vol);
	setVolume(s);   // wie cli.vol("n")
	wsVol(s);       // Weboberfläche aktualisieren
}

static void playKey(int n)
{
	char s[4];
	if (btSpeakerActive()) btSpeakerToRadio();   // Handy trennen, zurück zum Radio
	sprintf(s, "%d", n);
	printf("stationkeys: Taste %d -> Sender %d\n", n, n);
	playStation(s); // wie cli.play("n"), merkt sich den Sender
}

static void setMute(bool on)
{
	renderer_set_mute(on);
	printf("stationkeys: Mute %s\n", on ? "an" : "aus");
}

static bool configAdc(gpio_num_t pin, adc1_channel_t *chan, const char *name)
{
	int ch = adcChannel(pin);
	if (ch < 0)
	{
		ESP_LOGE(TAG, "%s GPIO %d ist kein ADC1-Pin (32..39)", name, pin);
		return false;
	}
	*chan = ch;
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(ch, ADC_ATTEN_DB_11);
	printf("stationkeys: %s an GPIO %d (ADC1 Kanal %d)\n", name, pin, ch);
	return true;
}

static void task_stationkeys(void *pvParams)
{
	uint8_t same[NB_KEYS] = {0};
	// Zu Beginn gilt jede Taste als gedrückt: Sie löst erst aus, wenn sie
	// einmal losgelassen gesehen wurde (kein Fehlalarm beim Start).
	// Bei Rasttasten spielt KaRadio beim Start den gespeicherten Sender.
	bool pressed[NB_KEYS], lastRead[NB_KEYS];
	for (int i = 0; i < NB_KEYS; i++) { pressed[i] = true; lastRead[i] = true; }
	int lastVol = -1;
	int muteRef = -1;          // Bezugswert für die Drehrichtung
	bool muted = false;
	uint32_t tick = 0;

	while (1)
	{
		// --- Sendertasten ---
		for (int i = 0; i < NB_KEYS; i++)
		{
			if (keyPin[i] == GPIO_NONE) continue;
			bool down = (gpio_get_level(keyPin[i]) == 0);
			if (down == lastRead[i]) { if (same[i] < KEY_STABLE) same[i]++; }
			else { same[i] = 0; lastRead[i] = down; }
			if ((same[i] >= KEY_STABLE) && (down != pressed[i]))
			{
				pressed[i] = down;
				if (down) playKey(i);
			}
		}

		// --- Bluetooth: Umschaltung Radio <-> Handy ---
		btSpeakerPoll();

		// --- Lautstärke-Poti ---
		if ((potPin != GPIO_NONE) && (tick % POT_EVERY == 0))
		{
			int vol = readPot();
			bool moved = (lastVol < 0) || (abs(vol - lastVol) >= VOL_HYST)
				|| ((vol != lastVol) && (vol == 0 || vol == VOL_MAX));
			// Kurz nach dem Start setzt KaRadio die gespeicherte Lautstärke.
			// In dieser Zeit stellt das Poti sie immer wieder richtig.
			if (moved) lastVol = vol;
			bool startup = (tick * LOOP_MS < VOL_FORCE_MS) && (getIvol() != lastVol);
			if (moved || startup)
			{
				ESP_LOGD(TAG, "Poti -> Lautstaerke %d", lastVol);
				applyVolume(lastVol);
			}
		}

		// --- Mute-Poti: Drehrichtung ---
		if ((mutePin != GPIO_NONE) && (tick % POT_EVERY == 4))
		{
			int m = readAdc(muteChan);
			if (muteRef < 0) muteRef = m;
			else if (!muted)
			{
				if (m > muteRef) muteRef = m;   // höchsten Punkt merken
				else if (muteRef - m >= MUTE_STEP) { muted = true; muteRef = m; setMute(true); }
			}
			else
			{
				if (m < muteRef) muteRef = m;   // tiefsten Punkt merken
				else if (m - muteRef >= MUTE_STEP) { muted = false; muteRef = m; setMute(false); }
			}
		}

		tick++;
		vTaskDelay(LOOP_MS / portTICK_PERIOD_MS);
	}
}

static bool configKey(gpio_num_t pin)
{
	if (!GPIO_IS_VALID_GPIO(pin)) return false;
	gpio_config_t io = {
		.pin_bit_mask = 1ULL << pin,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	gpio_config(&io);
	return true;
}

void stationKeysStart(void)
{
	readConfig();

	for (int i = 0; i < NB_KEYS; i++)
	{
		if (keyPin[i] == GPIO_NONE) continue;
		if (!configKey(keyPin[i])) { keyPin[i] = GPIO_NONE; continue; }
		printf("stationkeys: Taste %d an GPIO %d\n", i, keyPin[i]);
	}

	if ((potPin != GPIO_NONE) && !configAdc(potPin, &potChan, "Poti")) potPin = GPIO_NONE;

	if (mutePin != GPIO_NONE)
	{
		if (configAdc(mutePin, &muteChan, "Mute-Poti"))
		{
			// Pull-up, damit ein fehlendes Poti nichts auslöst (nach der
			// ADC-Einrichtung, die die Pulls abschaltet). GPIO 34..39 haben keinen.
			if (mutePin < 34) rtc_gpio_pullup_en(mutePin);
		}
		else mutePin = GPIO_NONE;
	}

	// Bluetooth immer an (sichtbar als "Biennophone")
	btSpeakerInit();

	// Der Task läuft immer: er bedient auch die Bluetooth-Umschaltung.
	xTaskCreatePinnedToCore(task_stationkeys, "task_stationkeys", 4096, NULL, PRIO_ADDON, NULL, CPU_ADDON);
}
