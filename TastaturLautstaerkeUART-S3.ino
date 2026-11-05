// Tastatur + Lautstärke-Poti -> KaRadio32 über UART
// Version für ESP32-S3 (abgeleitet von TastaturLautstaerkeUART.ino für den Pro Mini)
//
// Tasten (GPIO 4-9, gegen GND) -> cli.play("n")
// Poti an GPIO 1 (0-3,3 V)     -> cli.vol("0..254")
//
// Ausgabe über UART0 = zweiter USB-Port ("COM"/"UART") des S3.
// Dieselben Befehle liegen am Pin TX0 (GPIO 43) an.
//
// Verdrahtung ESP32-S3 <-> ESP32 WROVER (KaRadio):
//   S3 GPIO 43 (TX0) -> WROVER RX0 (GPIO 3)
//   GND              -> GND
//   Beide Boards haben 3,3 V -> kein Spannungsteiler nötig.
//   Hinweis: Ein USB-Kabel direkt zwischen S3 und WROVER funktioniert nicht
//   (beide sind USB-Geräte, keiner ist Host). Der COM-Port eignet sich zum
//   Mitlesen am PC, zum WROVER führt die Leitung GPIO 43 -> GPIO 3.
//
// Anzeige über die eingebaute RGB-LED:
//   - Farbe      = zuletzt gewählter Sender (6 Farben, vor der ersten Taste weiß)
//   - Helligkeit = Lautstärke (bei 0 fast aus, bei 254 hell)

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
bool startVolumeGesendet = false;

void sendeBefehl(const String &befehl) {
  Serial0.print(befehl);
  Serial0.print("\r\n");
}

void sendeStation(int nr) {
  sendeBefehl("cli.play(\"" + String(nr) + "\")");
}

void sendeLautstaerke(int vol) {
  sendeBefehl("cli.vol(\"" + String(vol) + "\")");
}

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

void setup() {
  Serial0.begin(UART_BAUD);   // UART0: COM-Port + GPIO 43 -> KaRadio

  analogReadResolution(12);
  angezeigteLautstaerke = leseLautstaerke();
  aktualisiereLed();

  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP);
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }
}

void loop() {
  // Lautstärke: erst nach dem Boot des WROVER senden, sonst geht der Wert verloren.
  // Die LED zeigt die Poti-Stellung aber schon vorher an.
  if (!startVolumeGesendet) {
    zeigeLautstaerke(leseLautstaerke());
    if (millis() > START_WARTEN) {
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
