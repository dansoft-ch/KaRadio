// Tastatur + Lautstärke-Poti -> KaRadio32 über UART
// Tasten (Pins 4-9, gegen GND) -> cli.play("n")
// Poti an A0 (0-3,3 V)         -> cli.vol("0..254")
//
// Verdrahtung Pro Mini <-> ESP32 (WROVER):
//   Pro Mini TX (D1) -> ESP32 RX0 (GPIO3)   (bei 5-V-Pro-Mini: Spannungsteiler!)
//   GND              -> GND
//   (ESP32 TX0 muss nicht angeschlossen werden)

const int schalterPins[6] = {4, 5, 6, 7, 8, 9};
int letzterZustand[6];

const int ledPin  = 13;     // eingebaute LED des Pro Mini
const int volPin  = A0;     // Schleifer des Lautstärke-Potis

const bool LED_FEEDBACK = true;    // nach dem Test auf false setzen

// Senderspeicher pro Taste (Taste 1 = Index 0 ... Taste 6 = Index 5).
// Bisher löste Taste 1 den IR-Code K_0 aus, also Station 0 -> Offset 0.
// Falls Taste 1 den ersten Sender (Nr. 1) abrufen soll: auf 1 setzen.
const int STATION_OFFSET = 0;

const long UART_BAUD = 115200;     // KaRadio-Standard

// Lautstärke
const int  VOL_MAX        = 254;   // KaRadio: 0..254
const int  VOL_HYSTERESE  = 2;     // Mindeständerung, bevor neu gesendet wird
const unsigned long VOL_INTERVALL = 80;     // ms zwischen ADC-Auswertungen
const unsigned long START_WARTEN  = 10000;  // ms, bis KaRadio gebootet hat
int  letzteLautstaerke = -100;
unsigned long letzteVolZeit = 0;
bool startVolumeGesendet = false;

void sendeStation(int nr) {
  Serial.print(F("cli.play(\""));
  Serial.print(nr);
  Serial.print(F("\")\r\n"));
}

void sendeLautstaerke(int vol) {
  Serial.print(F("cli.vol(\""));
  Serial.print(vol);
  Serial.print(F("\")\r\n"));
}

int leseLautstaerke() {
  long summe = 0;
  for (uint8_t i = 0; i < 8; i++) summe += analogRead(volPin);   // glätten
  int roh = summe / 8;                                           // 0..1023
  return map(roh, 0, 1023, 0, VOL_MAX);
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

void blinke(int anzahl) {
  for (int n = 0; n < anzahl; n++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(150);
  }
}

void setup() {
  Serial.begin(UART_BAUD);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP);
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }
}

void loop() {
  // Lautstärke: erst nach dem Boot des ESP32 senden, sonst geht der Wert verloren
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
