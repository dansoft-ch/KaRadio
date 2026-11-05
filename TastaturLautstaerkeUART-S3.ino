// Tastatur + Lautstärke-Poti -> KaRadio32 über UART
// Version für ESP32-S3 (abgeleitet von TastaturLautstaerkeUART.ino für den Pro Mini)
//
// Tasten (GPIO 4-9, gegen GND) -> cli.play("n")
// Poti an GPIO 1 (0-3,3 V)     -> cli.vol("0..254")
//
// Verdrahtung ESP32-S3 <-> ESP32 WROVER (KaRadio):
//   S3 GPIO 17 (TX, Serial1) -> WROVER RX0 (GPIO 3)
//   GND                      -> GND
//   Beide Boards haben 3,3 V -> kein Spannungsteiler nötig.
//   (WROVER TX0 muss nicht angeschlossen werden)
//
// Unterschiede zur Pro-Mini-Version:
//   - Befehle gehen über Serial1 (fester TX-Pin), unabhängig von "USB CDC On Boot".
//     Serial (USB) bleibt frei für Debug-Ausgaben.
//   - ADC hat 12 Bit (0-4095) statt 10 Bit.
//   - Rückmeldung über die eingebaute RGB-LED statt Pin 13.

const int schalterPins[6] = {4, 5, 6, 7, 8, 9};
int letzterZustand[6];

const int volPin   = 1;     // Schleifer des Lautstärke-Potis (ADC1)
const int UART_TX  = 17;    // -> WROVER RX0 (GPIO 3)
const int UART_RX  = 18;    // wird nicht gebraucht, muss aber angegeben werden

const bool LED_FEEDBACK = true;    // nach dem Test auf false setzen
const bool DEBUG_USB    = true;    // gesendete Befehle zusätzlich auf USB (Serial) ausgeben

// Senderspeicher pro Taste (Taste 1 = Index 0 ... Taste 6 = Index 5).
// Falls Taste 1 den ersten Sender (Nr. 1) abrufen soll: auf 1 setzen.
const int STATION_OFFSET = 0;

const long UART_BAUD = 115200;     // KaRadio-Standard

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
  Serial1.print(befehl);
  Serial1.print("\r\n");
  if (DEBUG_USB) Serial.println(befehl);
}

void sendeStation(int nr) {
  sendeBefehl("cli.play(\"" + String(nr) + "\")");
}

void sendeLautstaerke(int vol) {
  sendeBefehl("cli.vol(\"" + String(vol) + "\")");
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
    letzteLautstaerke = vol;
  }
}

void setzeLed(uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_BUILTIN
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(RGB_BUILTIN, r, g, b);
  #else
    neopixelWrite(RGB_BUILTIN, r, g, b);
  #endif
#endif
}

void blinke(int anzahl) {
  for (int n = 0; n < anzahl; n++) {
    setzeLed(0, 0, 40);   // blau, gedimmt
    delay(100);
    setzeLed(0, 0, 0);
    delay(150);
  }
}

void setup() {
  Serial.begin(115200);                                     // USB (Debug)
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);   // -> KaRadio

  analogReadResolution(12);
  setzeLed(0, 0, 0);

  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP);
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }
}

void loop() {
  // Lautstärke: erst nach dem Boot des WROVER senden, sonst geht der Wert verloren
  if (!startVolumeGesendet && millis() > START_WARTEN) {
    letzteLautstaerke = leseLautstaerke();
    sendeLautstaerke(letzteLautstaerke);
    startVolumeGesendet = true;
  }
  if (startVolumeGesendet) pruefeLautstaerke();

  // Tasten
  for (int i = 0; i < 6; i++) {
    int aktuell = digitalRead(schalterPins[i]);

    if (aktuell != letzterZustand[i]) {
      delay(50);                       // Entprellen
      aktuell = digitalRead(schalterPins[i]);

      // nur senden, wenn der Kontakt neu geschlossen wurde
      if (aktuell == LOW && letzterZustand[i] == HIGH) {
        sendeStation(i + STATION_OFFSET);
        if (LED_FEEDBACK) blinke(i + 1);   // Taste 1 = 1x, ..., Taste 6 = 6x
      }
      letzterZustand[i] = aktuell;
    }
  }
}
