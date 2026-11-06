/*
 * stationkeys.c - Ergänzung für KaRadio32 (dansoft-ch/KaRadio)
 *
 * Bis zu 6 Taster wählen direkt einen Sender (Taste n -> Sender n),
 * ein Poti stellt die Lautstärke (0..254) ein.
 * Das ersetzt den ESP32-S3, der bisher cli.play/cli.vol per UART geschickt hat.
 *
 * Konfiguration in der Hardware-Partition (gpio_space), z. B. in der .csv:
 *   P_KEY0 .. P_KEY5   GPIO des Tasters für Sender 0..5 (Taster gegen GND,
 *                      interner Pull-up). 255 = nicht benutzt.
 *   P_VOL_POT          GPIO des Poti-Schleifers, nur ADC1 (32..39).
 *                      Poti-Enden an 3,3 V und GND. 255 = nicht benutzt.
 * Fehlen die Einträge, ist die Funktion abgeschaltet.
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "nvs.h"
#include "gpio.h"
#include "app_main.h"
#include "webserver.h"
#include "webclient.h"
#include "stationkeys.h"

#define TAG "stationkeys"

extern const char hardware[];   // aus gpio.c
extern const char gpio_space[];

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

static gpio_num_t keyPin[NB_KEYS];
static gpio_num_t potPin = GPIO_NONE;
static adc1_channel_t potChan;

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

static void readConfig(void)
{
	nvs_handle handle;
	char key[8];
	for (int i = 0; i < NB_KEYS; i++) keyPin[i] = GPIO_NONE;
	potPin = GPIO_NONE;

	if (open_partition(hardware, gpio_space, NVS_READONLY, &handle) != ESP_OK) return;
	for (int i = 0; i < NB_KEYS; i++)
	{
		uint8_t v;
		sprintf(key, "P_KEY%d", i);
		if (nvs_get_u8(handle, key, &v) == ESP_OK) keyPin[i] = v;
	}
	uint8_t v;
	if (nvs_get_u8(handle, "P_VOL_POT", &v) == ESP_OK) potPin = v;
	close_partition(handle, hardware);
}

static int readPot(void)
{
	int sum = 0;
	for (int i = 0; i < POT_SAMPLES; i++) sum += adc1_get_raw(potChan);
	int raw = sum / POT_SAMPLES;
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
	sprintf(s, "%d", n);
	ESP_LOGI(TAG, "Taste %d -> Sender %d", n, n);
	playStation(s); // wie cli.play("n"), merkt sich den Sender
}

static void task_stationkeys(void *pvParams)
{
	uint8_t same[NB_KEYS] = {0};
	bool pressed[NB_KEYS] = {false};
	bool lastRead[NB_KEYS] = {false};
	int lastVol = -1;
	uint32_t tick = 0;

	while (1)
	{
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

		tick++;
		vTaskDelay(LOOP_MS / portTICK_PERIOD_MS);
	}
}

void stationKeysStart(void)
{
	bool any = false;
	readConfig();

	for (int i = 0; i < NB_KEYS; i++)
	{
		if (keyPin[i] == GPIO_NONE) continue;
		if (!GPIO_IS_VALID_GPIO(keyPin[i])) { keyPin[i] = GPIO_NONE; continue; }
		gpio_config_t io = {
			.pin_bit_mask = 1ULL << keyPin[i],
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_ENABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
		};
		gpio_config(&io);
		ESP_LOGI(TAG, "Taste %d an GPIO %d", i, keyPin[i]);
		any = true;
	}

	if (potPin != GPIO_NONE)
	{
		int ch = adcChannel(potPin);
		if (ch < 0)
		{
			ESP_LOGE(TAG, "P_VOL_POT GPIO %d ist kein ADC1-Pin (32..39)", potPin);
			potPin = GPIO_NONE;
		}
		else
		{
			potChan = ch;
			adc1_config_width(ADC_WIDTH_BIT_12);
			adc1_config_channel_atten(potChan, ADC_ATTEN_DB_11);
			ESP_LOGI(TAG, "Poti an GPIO %d (ADC1 Kanal %d)", potPin, ch);
			any = true;
		}
	}

	if (!any) return; // nichts konfiguriert
	xTaskCreatePinnedToCore(task_stationkeys, "task_stationkeys", 3072, NULL, PRIO_ADDON, NULL, CPU_ADDON);
}
