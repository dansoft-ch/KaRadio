// Tastatur + Lautstärke-Poti -> KaRadio32 über USB (S3 als USB-Host)
// Version für ESP32-S3 (abgeleitet von TastaturLautstaerkeUART.ino für den Pro Mini)
//
// Tasten (GPIO 4-9, gegen GND) -> cli.play("n")
// Poti an GPIO 1 (0-3,3 V)     -> cli.vol("0..254")
//
// Ausgabe der Befehle:
//   1. USB-Port "USB" des S3 (USB-Host) -> USB-Port des WROVER-Boards.
//      Der S3 spricht dort direkt mit dem USB-Seriell-Chip des WROVER-Boards
//      (CP210x, CH340/CH341 oder CDC-Chip wie CH9102/CH343), 115200 Baud,
//      genau so, wie es vorher der serielle Monitor der Arduino-IDE am PC tat.
//   2. Zusätzlich wie bisher UART0 = USB-Port "COM" des S3 (und Pin TX0 = GPIO 43):
//      zum Mitlesen am PC. Dort erscheinen auch Meldungen zum USB-Host.
//
// Verbindung:
//   S3-Port "USB" --(USB-Kabel, ggf. OTG-Adapter)--> USB-Port des WROVER-Boards
//   Der S3 muss über den Port "USB" 5 V an das WROVER-Board liefern. Bei vielen
//   S3-Boards ist das erst nach Schließen einer Lötbrücke ("USB-OTG", "IN-OUT")
//   der Fall. Alternativ das WROVER-Board selbst mit Strom versorgen.
//
// Hinweis zu DTR/RTS: Die Leitungen des USB-Seriell-Chips werden absichtlich
// nicht angefasst – auf ESP32-Boards hängen daran EN und GPIO0 (Auto-Reset).
//
// I2S-Ausgang (Ton) – liegt am WROVER, nicht am S3 (aus standard_psram_dan.csv):
//   WROVER GPIO 25 (P_I2S_LRCK) -> MAX98357A LRC   (Kabel grün)
//   WROVER GPIO 26 (P_I2S_BCLK) -> MAX98357A BCLK  (Kabel blau)
//   WROVER GPIO 22 (P_I2S_DATA) -> MAX98357A DIN   (Kabel gelb)
//   5 V                         -> MAX98357A VIN   (Kabel rot)
//   GND                         -> MAX98357A GND   (Kabel schwarz)
//
// Anzeige über die eingebaute RGB-LED:
//   - Farbe      = zuletzt gewählter Sender (6 Farben, vor der ersten Taste weiß)
//   - Helligkeit = Lautstärke (bei 0 fast aus, bei 254 hell)
//
// Arduino-IDE: "USB CDC On Boot" = Disabled (Standard), damit Serial0 = COM-Port.

#include "usb/usb_host.h"

const int schalterPins[6] = {4, 5, 6, 7, 8, 9};
int letzterZustand[6];

const int volPin = 1;       // Schleifer des Lautstärke-Potis (ADC1)

// Senderspeicher pro Taste (Taste 1 = Index 0 ... Taste 6 = Index 5).
// Falls Taste 1 den ersten Sender (Nr. 1) abrufen soll: auf 1 setzen.
const int STATION_OFFSET = 0;

const long UART_BAUD = 115200;     // KaRadio-Standard

// Helligkeit der RGB-LED (0..255) – die LED ist sehr hell, daher nicht voll ausfahren
const uint8_t LED_HELL_MIN = 2;     // bei Lautstärke 0 (Farbe gerade noch erkennbar)
const uint8_t LED_HELL_MAX = 120;   // bei Lautstärke 254

// Senderfarben (R, G, B) für Taste 1 ... 6
const uint8_t farbeOhneSender[3] = {255, 255, 255};   // weiß, bis eine Taste gedrückt wurde
int aktuellerSender = -1;                             // -1 = noch keine Taste
int angezeigteLautstaerke = 0;
const uint8_t senderFarben[6][3] = {
  {255,   0,   0},   // Taste 1: rot
  {  0, 255,   0},   // Taste 2: grün
  {  0,   0, 255},   // Taste 3: blau
  {255, 180,   0},   // Taste 4: gelb
  {  0, 255, 255},   // Taste 5: cyan
  {255,   0, 255},   // Taste 6: magenta
};

// Lautstärke
const int  ADC_MAX        = 4095;  // ESP32-S3: 12 Bit
const int  ADC_TOTZONE    = 60;    // Totzone an den Enden (ADC des S3 ist dort ungenau)
const int  VOL_MAX        = 254;   // KaRadio: 0..254
const int  VOL_HYSTERESE  = 2;     // Mindeständerung, bevor neu gesendet wird
const unsigned long VOL_INTERVALL = 80;     // ms zwischen ADC-Auswertungen
const unsigned long START_WARTEN  = 10000;  // ms, bis KaRadio gebootet hat
int  letzteLautstaerke = -100;
unsigned long letzteVolZeit = 0;
unsigned long startZeit = 0;       // ab hier läuft START_WARTEN (neu bei USB-Verbindung)
bool startVolumeGesendet = false;

// ===================== USB-Host =====================

struct Befehl { char text[48]; };
QueueHandle_t befehlQueue;

static usb_host_client_handle_t usbClient;
static usb_device_handle_t usbGeraet = NULL;
static usb_transfer_t *ctrlXfer = NULL;
static usb_transfer_t *bulkXfer = NULL;
static uint8_t datenIntf = 0;
static uint8_t epOut = 0;
static volatile uint8_t neueAdresse = 0;
static volatile bool geraetWeg = false;
static volatile bool ctrlFertig = false;
static volatile bool bulkBusy = false;
static volatile bool usbBereit = false;
static volatile bool usbNeuVerbunden = false;

void usbClientCallback(const usb_host_client_event_msg_t *msg, void *arg) {
  if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
    neueAdresse = msg->new_dev.address;
  } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
    geraetWeg = true;
  }
}

void ctrlCallback(usb_transfer_t *t) { ctrlFertig = true; }
void bulkCallback(usb_transfer_t *t) { bulkBusy = false; }

// Control-Transfer an Endpunkt 0, wartet auf das Ende.
bool steuerTransfer(uint8_t reqType, uint8_t req, uint16_t value, uint16_t index,
                    uint8_t *daten, uint16_t laenge) {
  usb_setup_packet_t *s = (usb_setup_packet_t *)ctrlXfer->data_buffer;
  s->bmRequestType = reqType;
  s->bRequest = req;
  s->wValue = value;
  s->wIndex = index;
  s->wLength = laenge;
  bool rein = reqType & 0x80;
  if (daten && !rein) memcpy(ctrlXfer->data_buffer + sizeof(usb_setup_packet_t), daten, laenge);
  ctrlXfer->num_bytes = sizeof(usb_setup_packet_t) + laenge;
  ctrlXfer->device_handle = usbGeraet;
  ctrlXfer->bEndpointAddress = 0;
  ctrlXfer->callback = ctrlCallback;
  ctrlXfer->context = NULL;
  ctrlXfer->timeout_ms = 1000;

  ctrlFertig = false;
  if (usb_host_transfer_submit_control(usbClient, ctrlXfer) != ESP_OK) return false;
  while (!ctrlFertig) usb_host_client_handle_events(usbClient, pdMS_TO_TICKS(10));
  if (ctrlXfer->status != USB_TRANSFER_STATUS_COMPLETED) return false;
  if (daten && rein) memcpy(daten, ctrlXfer->data_buffer + sizeof(usb_setup_packet_t), laenge);
  return true;
}

// CDC-ACM (z. B. CH9102, CH343): SET_LINE_CODING 115200 8N1
bool einrichtenCdc(uint8_t kommIntf) {
  uint8_t lc[7] = {
    (uint8_t)(UART_BAUD), (uint8_t)(UART_BAUD >> 8), (uint8_t)(UART_BAUD >> 16), (uint8_t)(UART_BAUD >> 24),
    0, 0, 8   // 1 Stoppbit, keine Parität, 8 Datenbits
  };
  return steuerTransfer(0x21, 0x20, 0, kommIntf, lc, sizeof(lc));
}

// Silicon Labs CP210x
bool einrichtenCp210x(uint8_t intf) {
  uint8_t baud[4] = {
    (uint8_t)(UART_BAUD), (uint8_t)(UART_BAUD >> 8), (uint8_t)(UART_BAUD >> 16), (uint8_t)(UART_BAUD >> 24)
  };
  return steuerTransfer(0x41, 0x00, 0x0001, intf, NULL, 0)         // IFC_ENABLE
      && steuerTransfer(0x41, 0x1E, 0, intf, baud, 4)              // SET_BAUDRATE
      && steuerTransfer(0x41, 0x03, 0x0800, intf, NULL, 0);        // SET_LINE_CTL 8N1
}

// WCH CH340/CH341 (Ablauf wie im Linux-Treiber ch341.c)
bool einrichtenCh34x() {
  uint8_t version[2] = {0, 0};
  if (!steuerTransfer(0xC0, 0x5F, 0, 0, version, 2)) return false;   // Version lesen
  if (!steuerTransfer(0x40, 0xA1, 0, 0, NULL, 0)) return false;      // Init
  // Teiler für 115200 Baud: Prescaler 3, Divisor 0xCC, Bit 7 = sofort senden
  if (!steuerTransfer(0x40, 0x9A, 0x1312, 0xCC83, NULL, 0)) return false;
  if (version[0] >= 0x30) {
    // LCR: RX + TX an, 8 Datenbits, keine Parität, 1 Stoppbit
    if (!steuerTransfer(0x40, 0x9A, 0x2518, 0x00C3, NULL, 0)) return false;
  }
  return true;
}

void geraetEinrichten(uint8_t adresse) {
  if (usb_host_device_open(usbClient, adresse, &usbGeraet) != ESP_OK) {
    usbGeraet = NULL;
    Serial0.println("[USB] Geraet konnte nicht geoeffnet werden");
    return;
  }
  const usb_device_desc_t *dd;
  const usb_config_desc_t *cd;
  usb_host_get_device_descriptor(usbGeraet, &dd);
  usb_host_get_active_config_descriptor(usbGeraet, &cd);
  Serial0.printf("[USB] Geraet angeschlossen: VID %04X PID %04X\r\n", dd->idVendor, dd->idProduct);

  // Konfigurationsdeskriptor durchgehen: Bulk-OUT-Endpunkt und ggf. CDC-Interface suchen
  const uint8_t *p = (const uint8_t *)cd;
  int gesamt = cd->wTotalLength;
  int aktIntf = -1, aktAlt = 0, kommIntf = -1;
  epOut = 0;
  for (int i = 0; i + 1 < gesamt && p[i] >= 2; i += p[i]) {
    if (p[i + 1] == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
      aktIntf = p[i + 2];
      aktAlt = p[i + 3];
      if (p[i + 5] == 0x02 && kommIntf < 0) kommIntf = aktIntf;   // CDC Communication
    } else if (p[i + 1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT && aktAlt == 0 && epOut == 0) {
      bool bulk = (p[i + 3] & 0x03) == 0x02;
      bool raus = !(p[i + 2] & 0x80);
      if (bulk && raus) {
        epOut = p[i + 2];
        datenIntf = aktIntf;
      }
    }
  }
  if (epOut == 0) {
    Serial0.println("[USB] Kein Bulk-OUT-Endpunkt gefunden - kein USB-Seriell-Chip?");
    usb_host_device_close(usbClient, usbGeraet);
    usbGeraet = NULL;
    return;
  }
  if (usb_host_interface_claim(usbClient, usbGeraet, datenIntf, 0) != ESP_OK) {
    Serial0.println("[USB] Interface konnte nicht belegt werden");
    usb_host_device_close(usbClient, usbGeraet);
    usbGeraet = NULL;
    return;
  }

  bool ok;
  const char *typ;
  if (kommIntf >= 0) {
    typ = "CDC-ACM";
    ok = einrichtenCdc(kommIntf);
  } else if (dd->idVendor == 0x10C4) {
    typ = "CP210x";
    ok = einrichtenCp210x(datenIntf);
  } else if (dd->idVendor == 0x1A86) {
    typ = "CH34x";
    ok = einrichtenCh34x();
  } else {
    typ = "unbekannt (Daten werden ohne Einrichtung gesendet)";
    ok = true;
  }
  Serial0.printf("[USB] Chip: %s, Einrichtung %s\r\n", typ, ok ? "ok" : "FEHLGESCHLAGEN");

  usbBereit = true;          // auch bei Fehler versuchen zu senden
  usbNeuVerbunden = true;
}

void geraetSchliessen() {
  if (usbGeraet) {
    usb_host_interface_release(usbClient, usbGeraet, datenIntf);
    usb_host_device_close(usbClient, usbGeraet);
    usbGeraet = NULL;
    Serial0.println("[USB] Geraet getrennt");
  }
}

// Verwaltet den USB-Host (Treiber-Ereignisse)
void usbLibTask(void *arg) {
  while (true) {
    uint32_t flags;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
  }
}

// Unser USB-Client: Gerät einrichten und Befehle senden
void usbClientTask(void *arg) {
  Befehl b;
  while (true) {
    usb_host_client_handle_events(usbClient, pdMS_TO_TICKS(5));

    if (geraetWeg) {
      usbBereit = false;
      if (!bulkBusy) {          // laufenden Transfer erst zu Ende kommen lassen
        geraetWeg = false;
        geraetSchliessen();
      }
      continue;
    }

    if (neueAdresse != 0 && usbGeraet == NULL) {
      uint8_t a = neueAdresse;
      neueAdresse = 0;
      geraetEinrichten(a);
    }

    if (usbBereit && !bulkBusy && xQueueReceive(befehlQueue, &b, 0) == pdTRUE) {
      size_t n = strlen(b.text);
      memcpy(bulkXfer->data_buffer, b.text, n);
      bulkXfer->num_bytes = n;
      bulkXfer->device_handle = usbGeraet;
      bulkXfer->bEndpointAddress = epOut;
      bulkXfer->callback = bulkCallback;
      bulkXfer->context = NULL;
      bulkXfer->timeout_ms = 1000;
      bulkBusy = true;
      if (usb_host_transfer_submit(bulkXfer) != ESP_OK) bulkBusy = false;
    }
  }
}

void usbHostStarten() {
  befehlQueue = xQueueCreate(16, sizeof(Befehl));

  usb_host_config_t hostCfg = {};
  hostCfg.skip_phy_setup = false;
  hostCfg.intr_flags = ESP_INTR_FLAG_LEVEL1;
  if (usb_host_install(&hostCfg) != ESP_OK) {
    Serial0.println("[USB] usb_host_install fehlgeschlagen");
    return;
  }
  xTaskCreatePinnedToCore(usbLibTask, "usbLib", 4096, NULL, 2, NULL, 0);

  usb_host_client_config_t clientCfg = {};
  clientCfg.is_synchronous = false;
  clientCfg.max_num_event_msg = 5;
  clientCfg.async.client_event_callback = usbClientCallback;
  clientCfg.async.callback_arg = NULL;
  if (usb_host_client_register(&clientCfg, &usbClient) != ESP_OK) {
    Serial0.println("[USB] usb_host_client_register fehlgeschlagen");
    return;
  }
  usb_host_transfer_alloc(64 + sizeof(usb_setup_packet_t), 0, &ctrlXfer);
  usb_host_transfer_alloc(64, 0, &bulkXfer);

  xTaskCreatePinnedToCore(usbClientTask, "usbClient", 4096, NULL, 3, NULL, 0);
  Serial0.println("[USB] Host gestartet, warte auf WROVER am Port \"USB\"");
}

// ===================== Befehle =====================

void sendeBefehl(const String &befehl) {
  String zeile = befehl + "\r\n";
  Serial0.print(zeile);                     // COM-Port / GPIO 43 (Mitlesen)

  if (usbBereit) {                          // USB -> WROVER
    Befehl b;
    strncpy(b.text, zeile.c_str(), sizeof(b.text) - 1);
    b.text[sizeof(b.text) - 1] = '\0';
    xQueueSend(befehlQueue, &b, 0);
  }
}

void sendeStation(int nr) {
  sendeBefehl("cli.play(\"" + String(nr) + "\")");
}

void sendeLautstaerke(int vol) {
  sendeBefehl("cli.vol(\"" + String(vol) + "\")");
}

// ===================== LED =====================

void setzeRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(RGB_BUILTIN, r, g, b);
  #else
    neopixelWrite(RGB_BUILTIN, r, g, b);
  #endif
#endif
}

// Farbe des Senders, Helligkeit nach Lautstärke.
// Quadratische Kennlinie, damit die Helligkeit gleichmäßig zunimmt.
void aktualisiereLed() {
  const uint8_t *farbe = (aktuellerSender < 0) ? farbeOhneSender
                                               : senderFarben[aktuellerSender];
  long v = angezeigteLautstaerke;
  long hell = LED_HELL_MIN + (LED_HELL_MAX - LED_HELL_MIN) * v * v / ((long)VOL_MAX * VOL_MAX);
  setzeRgb(farbe[0] * hell / 255, farbe[1] * hell / 255, farbe[2] * hell / 255);
}

void zeigeSender(int taste) {
  aktuellerSender = taste;
  aktualisiereLed();
}

void zeigeLautstaerke(int vol) {
  if (vol == angezeigteLautstaerke) return;
  angezeigteLautstaerke = vol;
  aktualisiereLed();
}

// ===================== Poti =====================

int leseLautstaerke() {
  long summe = 0;
  for (uint8_t i = 0; i < 16; i++) summe += analogRead(volPin);   // glätten
  int roh = summe / 16;                                            // 0..4095
  roh = constrain(roh, ADC_TOTZONE, ADC_MAX - ADC_TOTZONE);
  return map(roh, ADC_TOTZONE, ADC_MAX - ADC_TOTZONE, 0, VOL_MAX);
}

void pruefeLautstaerke() {
  if (millis() - letzteVolZeit < VOL_INTERVALL) return;
  letzteVolZeit = millis();

  int vol = leseLautstaerke();
  bool aenderung = abs(vol - letzteLautstaerke) >= VOL_HYSTERESE;
  bool endanschlag = (vol != letzteLautstaerke) && (vol == 0 || vol == VOL_MAX);

  if (aenderung || endanschlag) {
    sendeLautstaerke(vol);
    zeigeLautstaerke(vol);
    letzteLautstaerke = vol;
  }
}

// ===================== Setup / Loop =====================

void setup() {
  Serial0.begin(UART_BAUD);   // UART0: COM-Port + GPIO 43 (Mitlesen)

  analogReadResolution(12);
  angezeigteLautstaerke = leseLautstaerke();
  aktualisiereLed();

  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP);
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }

  usbHostStarten();
}

void loop() {
  // WROVER (neu) per USB verbunden: Lautstärke nach dessen Boot erneut senden
  if (usbNeuVerbunden) {
    usbNeuVerbunden = false;
    startZeit = millis();
    startVolumeGesendet = false;
  }

  // Lautstärke: erst nach dem Boot des WROVER senden, sonst geht der Wert verloren.
  // Die LED zeigt die Poti-Stellung aber schon vorher an.
  if (!startVolumeGesendet) {
    zeigeLautstaerke(leseLautstaerke());
    if (millis() - startZeit > START_WARTEN) {
      letzteLautstaerke = leseLautstaerke();
      sendeLautstaerke(letzteLautstaerke);
      zeigeLautstaerke(letzteLautstaerke);
      startVolumeGesendet = true;
    }
  } else {
    pruefeLautstaerke();
  }

  // Tasten
  for (int i = 0; i < 6; i++) {
    int aktuell = digitalRead(schalterPins[i]);

    if (aktuell != letzterZustand[i]) {
      delay(50);                       // Entprellen
      aktuell = digitalRead(schalterPins[i]);

      // nur senden, wenn der Kontakt neu geschlossen wurde
      if (aktuell == LOW && letzterZustand[i] == HIGH) {
        sendeStation(i + STATION_OFFSET);
        zeigeSender(i);                // Farbe bleibt bis zur nächsten Taste
      }
      letzterZustand[i] = aktuell;
    }
  }
}
