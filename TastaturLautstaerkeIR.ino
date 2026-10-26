// Tastatur + Lautstärke-Poti -> NEC-Signal direkt an ESP32-IR-Pin
// Ausgang wie ein IR-Empfängermodul: Ruhe = HIGH, Impuls = LOW (kein 38-kHz-Träger)
//
// Tasten: 6 Schalter (Pins 4-9, gegen GND) -> Sendervorwahl (unverändert)
// Lautstärke: Poti-Schleifer an A0 (0 ... ~3,2 V) -> Lauter/Leiser-Befehle per IR
//
// Anti-Drift-Konzept (IR kennt nur "lauter/leiser", keinen Absolutwert):
//  1. Beim Start fährt der Sketch die Radio-Lautstärke einmal sicher auf 0
//     und stellt dann die Poti-Position nach  -> beide Seiten sind synchron.
//  2. Der Sketch zählt mit (kaVol) und schickt Schritt für Schritt die Differenz
//     zwischen Poti-Position und gezählter Lautstärke.
//  3. Wird Anschlag "null" oder "voll" erreicht, werden ein paar Zusatzbefehle
//     geschickt. KaRadio begrenzt bei 0 bzw. Maximum -> verlorene Impulse
//     werden bei jedem Anschlag automatisch korrigiert, null ist immer erreichbar.
//  4. Mittelwert + Hysterese + Totzonen an den Enden: kein Zittern, kein Wandern.

const int schalterPins[6] = {4, 5, 6, 7, 8, 9};
int letzterZustand[6];

const int ausgangPin = 3;   // -> IR-Pin (P_IR_SIGNAL) des ESP32
const int ledPin = 13;      // eingebaute LED des Pro Mini
const int potPin = A0;      // Lautstärke-Poti (Schleifer)

const bool LED_FEEDBACK = true;   // nach dem Test auf false setzen
const bool DEBUG_SERIAL = true;   // 9600 Baud: Poti-Rohwert, Ziel, Zähler (zum Kalibrieren)

// NEC-Befehle, Adresse 0x00 -> KaRadio-Codes 0xFF00xx
const uint8_t befehle[6] = {0x0C, 0x18, 0x5E, 0x08, 0x1C, 0x5A};

// Lautstärke-Befehle -> in KaRadio-CSV eintragen: K_UP = 0xFF0040, K_DOWN = 0xFF0044
const uint8_t BEFEHL_LAUTER = 0x40;
const uint8_t BEFEHL_LEISER = 0x44;

// ---- Einstellungen Lautstärke ----
const int  VOL_STUFEN    = 20;    // Anzahl Schritte von 0 bis Maximum (ggf. anpassen)
const int  RESYNC_EXTRA  = 6;     // Zusatzbefehle am Anschlag (Korrektur verlorener Impulse)
const int  ADC_MIN       = 0;     // ADC-Wert bei Poti ganz zu
const int  ADC_MAX       = 990;   // ADC-Wert bei Poti ganz auf (Pro Mini 3,3 V: ~990; bei 5 V: ~655)
const int  TOTZONE       = 20;    // ADC-Werte an beiden Enden, die sicher als 0 bzw. Max gelten
const float HYSTERESE    = 0.2;   // Poti muss 0,8 Schritte "über die Mitte" gehen, bevor das Ziel wechselt
const unsigned long MIN_ABSTAND_MS  = 10;    // Mindestabstand zwischen IR-Telegrammen
const unsigned long START_WARTEN_MS = 10000;  // so lange booten ESP32/KaRadio (Start-Sync erst danach)

// ---- Zustand Lautstärke ----
int  kaVol = 0;              // von uns mitgezählte KaRadio-Lautstärke (0 ... VOL_STUFEN)
int  zielVol = 0;            // gewünschte Lautstärke laut Poti
bool zielGesetzt = false;
bool syncFertig = false;     // Start-Synchronisation abgeschlossen?
int  syncRest = -1;          // verbleibende Leiser-Befehle beim Start-Sync
bool randErledigt = false;   // Zusatzbefehle am Anschlag schon gesendet?
int  randRest = 0;
float potFilter = -1;        // geglätteter ADC-Wert
unsigned long letzteSendung = 0;

void sendeByte(uint8_t b) {            // LSB zuerst
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(ausgangPin, LOW);     // Impuls 560 us
    delayMicroseconds(560);
    digitalWrite(ausgangPin, HIGH);    // Pause: 560 us = 0, 1690 us = 1
    delayMicroseconds((b & 1) ? 1690 : 560);
    b >>= 1;
  }
}

void sendeNEC(uint8_t adresse, uint8_t befehl) {
  noInterrupts();
  digitalWrite(ausgangPin, LOW);       // Startimpuls 9 ms
  delayMicroseconds(9000);
  digitalWrite(ausgangPin, HIGH);      // Startpause 4,5 ms
  delayMicroseconds(4500);
  sendeByte(adresse);
  sendeByte((uint8_t)~adresse);
  sendeByte(befehl);
  sendeByte((uint8_t)~befehl);
  digitalWrite(ausgangPin, LOW);       // Stopp-Impuls
  delayMicroseconds(560);
  digitalWrite(ausgangPin, HIGH);      // zurück in Ruhezustand
  interrupts();
}

// Alle Telegramme (Tasten und Lautstärke) laufen hierüber -> Mindestabstand garantiert
void sendeMitAbstand(uint8_t befehl) {
  while (millis() - letzteSendung < MIN_ABSTAND_MS) { }
  sendeNEC(0x00, befehl);
  letzteSendung = millis();
}

void blinke(int anzahl) {
  for (int n = 0; n < anzahl; n++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(150);
  }
}

// Poti lesen, glätten, in Zielstufe 0 ... VOL_STUFEN umrechnen
void potAuswerten() {
  long summe = 0;
  analogRead(potPin);                          // erste Messung verwerfen (Multiplexer einschwingen)
  for (int i = 0; i < 16; i++) summe += analogRead(potPin);
  float roh = summe / 16.0;

  if (potFilter < 0) potFilter = roh;
  else potFilter += 0.5 * (roh - potFilter);   // träger Mittelwert gegen Rauschen

  float von = ADC_MIN + TOTZONE;
  float bis = ADC_MAX - TOTZONE;
  float f = (potFilter - von) * VOL_STUFEN / (bis - von);
  if (f < 0) f = 0;
  if (f > VOL_STUFEN) f = VOL_STUFEN;

  if (!zielGesetzt) {
    zielVol = (int)(f + 0.5);
    zielGesetzt = true;
  } else if (f > zielVol + 0.5 + HYSTERESE || f < zielVol - 0.5 - HYSTERESE) {
    zielVol = (int)(f + 0.5);
  }

  if (DEBUG_SERIAL) {
    static unsigned long letzteAusgabe = 0;
    if (millis() - letzteAusgabe > 500) {
      letzteAusgabe = millis();
      Serial.print(F("ADC="));  Serial.print(potFilter, 0);
      Serial.print(F(" ziel=")); Serial.print(zielVol);
      Serial.print(F(" kaVol=")); Serial.println(kaVol);
    }
  }
}

// Sendet höchstens EIN Telegramm pro Aufruf -> Tasten bleiben ansprechbar
void lautstaerkeTask() {
  potAuswerten();

  if (millis() - letzteSendung < MIN_ABSTAND_MS) return;

  // 1) Start-Synchronisation: sicher auf 0 fahren
  if (!syncFertig) {
    if (millis() < START_WARTEN_MS) return;
    if (syncRest < 0) syncRest = VOL_STUFEN + RESYNC_EXTRA;
    sendeMitAbstand(BEFEHL_LEISER);
    syncRest--;
    if (syncRest == 0) {
      kaVol = 0;
      syncFertig = true;
    }
    return;
  }

  // 2) Differenz zwischen Poti-Ziel und gezählter Lautstärke abarbeiten
  if (kaVol != zielVol) {
    if (kaVol < zielVol) { sendeMitAbstand(BEFEHL_LAUTER); kaVol++; }
    else                 { sendeMitAbstand(BEFEHL_LEISER); kaVol--; }
    randErledigt = false;
    randRest = 0;
    return;
  }

  // 3) Am Anschlag (0 oder Maximum): Zähler gegen KaRadio-Begrenzung abgleichen
  if (!randErledigt && (zielVol == 0 || zielVol == VOL_STUFEN)) {
    randRest = RESYNC_EXTRA;
    randErledigt = true;
  }
  if (randRest > 0) {
    sendeMitAbstand(zielVol == 0 ? BEFEHL_LEISER : BEFEHL_LAUTER);
    randRest--;
  }
}

void setup() {
  pinMode(ausgangPin, OUTPUT);
  digitalWrite(ausgangPin, HIGH);      // Ruhepegel HIGH

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP);
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }

  if (DEBUG_SERIAL) Serial.begin(9600);
}

void loop() {
  for (int i = 0; i < 6; i++) {
    int aktuell = digitalRead(schalterPins[i]);

    if (aktuell != letzterZustand[i]) {
      delay(50);                       // Entprellen
      aktuell = digitalRead(schalterPins[i]);

      // nur senden, wenn der Kontakt neu geschlossen wurde
      if (aktuell == LOW && letzterZustand[i] == HIGH) {
        sendeMitAbstand(befehle[i]);
        if (LED_FEEDBACK) blinke(i + 1);   // Taste 1 = 1x, ..., Taste 6 = 6x
      }
      letzterZustand[i] = aktuell;
    }
  }

  lautstaerkeTask();
}
