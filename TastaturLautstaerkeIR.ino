// Tastatur + Lautstärke-Poti -> NEC-Signal direkt an ESP32-IR-Pin
// Ausgang wie ein IR-Empfängermodul: Ruhe = HIGH, Impuls = LOW (kein 38-kHz-Träger)
// Funktionsfähige und getestete Version 30.10.2026 (Achtung: werden RX Kanal gebraucht funktioniert das nicht mit alleb Boards, da sich die UART Kanäle blockieren. Von 3 Board lief es nur bedingt auf einem!!)
// Tasten: 6 Schalter (Pins 4-9, gegen GND) -> Sendervorwahl (unverändert)
// Lautstärke: Poti-Schleifer an A0 (0 ... ~3,2 V) -> Lauter/Leiser-Befehle per IR
//
// Anti-Drift-Konzept (IR kennt nur "lauter/leiser", keinen Absolutwert):
//  1. KaRadio ändert die Lautstärke pro IR-Befehl um 5 im Bereich 0...254 und begrenzt
//     an den Enden. Der Sketch bildet GENAU das nach (kaVol = 0...254, Schritt 5, Begrenzung).
//     -> Der gezählte Wert stimmt auch nach einem Vollausschlag mit KaRadio überein.
//     (Früher: 40 Stufen angenommen, KaRadio braucht aber 51 -> die Zusatzbefehle am
//      oberen Anschlag haben die Radio-Lautstärke jedes Mal weiter verschoben.)
//  2. Beim Start fährt der Sketch die Radio-Lautstärke einmal sicher auf 0
//     und stellt dann die Poti-Position nach -> beide Seiten sind synchron.
//  3. An beiden Anschlägen (0 und 254) werden ein paar Zusatzbefehle geschickt. KaRadio
//     begrenzt dort, der Zähler ändert sich nicht -> verlorene Impulse werden korrigiert,
//     Minimum (0) und Maximum (254) sind immer exakt gleich.
//  4. Mittelwert + Schaltschwelle + Totzonen an den Enden: kein Zittern, kein Wandern.
//  Pin Belegung Verstärker MAx98357A zu ESP32 WROVER
//  grün an 25; blau an 26; rot an 5V; gelb an 22; schwarz an G
//  Input UART an ESP32: Signal vom Spannungsteiler an RX

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

// ---- KaRadio-Lautstärke (Modell) ----
const int KA_MAX     = 254;   // KaRadio-Maximum
const int KA_SCHRITT = 5;     // Änderung pro IR-Befehl (setRelVolume(+-5))
const int STUFEN_VOLL = (KA_MAX + KA_SCHRITT - 1) / KA_SCHRITT;   // = 51 Befehle von 0 bis Maximum

// ---- Einstellungen Lautstärke ----
const int  RESYNC_EXTRA  = 6;     // Zusatzbefehle am Anschlag (Korrektur verlorener Impulse)
const int  ADC_MIN       = 0;     // ADC-Wert bei Poti ganz zu
const int  ADC_MAX       = 990;   // ADC-Wert bei Poti ganz auf (Pro Mini 3,3 V: ~990; bei 5 V: ~655)
const int  TOTZONE       = 20;    // ADC-Werte an beiden Enden, die sicher als 0 bzw. Max gelten
const float SCHALTSCHWELLE = 3.5; // Abweichung (in KaRadio-Einheiten, 0...254), ab der ein Schritt gesendet wird
                                  // muss > 2,5 sein (halber Schritt), sonst pendelt die Lautstärke
const unsigned long MIN_ABSTAND_MS  = 10;    // Mindestabstand zwischen IR-Telegrammen
const unsigned long START_WARTEN_MS = 10000;  // so lange booten ESP32/KaRadio (Start-Sync erst danach)

// ---- Zustand Lautstärke ----
int   kaVol = 0;            // nachgebildete KaRadio-Lautstärke (0 ... KA_MAX)
float zielVol = 0;          // gewünschte Lautstärke laut Poti (0 ... KA_MAX)
bool  syncFertig = false;   // Start-Synchronisation abgeschlossen?
int   syncRest = -1;        // verbleibende Leiser-Befehle beim Start-Sync
bool  randErledigt = false; // Zusatzbefehle am Anschlag schon gesendet?
int   randRest = 0;
float potFilter = -1;       // geglätteter ADC-Wert
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

// Lauter/Leiser senden und KaRadio-Verhalten nachbilden (Schritt 5, Begrenzung 0 / 254)
void lauter() {
  sendeMitAbstand(BEFEHL_LAUTER);
  kaVol += KA_SCHRITT;
  if (kaVol > KA_MAX) kaVol = KA_MAX;
}

void leiser() {
  sendeMitAbstand(BEFEHL_LEISER);
  kaVol -= KA_SCHRITT;
  if (kaVol < 0) kaVol = 0;
}

void blinke(int anzahl) {
  for (int n = 0; n < anzahl; n++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(150);
  }
}

// Poti lesen, glätten, in Ziel-Lautstärke 0 ... KA_MAX umrechnen
void potAuswerten() {
  long summe = 0;
  analogRead(potPin);                          // erste Messung verwerfen (Multiplexer einschwingen)
  for (int i = 0; i < 16; i++) summe += analogRead(potPin);
  float roh = summe / 16.0;

  if (potFilter < 0) potFilter = roh;
  else potFilter += 0.5 * (roh - potFilter);   // träger Mittelwert gegen Rauschen

  float von = ADC_MIN + TOTZONE;
  float bis = ADC_MAX - TOTZONE;
  float f = (potFilter - von) * KA_MAX / (bis - von);
  if (f < 0) f = 0;                            // Totzone unten -> exakt 0
  if (f > KA_MAX) f = KA_MAX;                  // Totzone oben  -> exakt Maximum
  zielVol = f;

  if (DEBUG_SERIAL) {
    static unsigned long letzteAusgabe = 0;
    if (millis() - letzteAusgabe > 500) {
      letzteAusgabe = millis();
      Serial.print(F("ADC="));  Serial.print(potFilter, 0);
      Serial.print(F(" ziel=")); Serial.print(zielVol, 0);
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
    if (syncRest < 0) syncRest = STUFEN_VOLL + RESYNC_EXTRA;
    sendeMitAbstand(BEFEHL_LEISER);
    syncRest--;
    if (syncRest == 0) {
      kaVol = 0;
      syncFertig = true;
    }
    return;
  }

  // 2) Abweichung zwischen Poti-Ziel und nachgebildeter Lautstärke abarbeiten
  //    An den Enden (Poti in der Totzone) wird exakt bis 0 bzw. Maximum gefahren,
  //    dazwischen erst ab SCHALTSCHWELLE -> kein Pendeln.
  bool hoch   = (zielVol >= KA_MAX) ? (kaVol < KA_MAX) : (zielVol - kaVol > SCHALTSCHWELLE);
  bool runter = (zielVol <= 0)      ? (kaVol > 0)      : (kaVol - zielVol > SCHALTSCHWELLE);

  if (hoch || runter) {
    if (hoch) lauter(); else leiser();
    randErledigt = false;
    randRest = 0;
    return;
  }

  // 3) Am Anschlag (0 oder Maximum): Zähler gegen KaRadio-Begrenzung abgleichen
  bool amEnde = (zielVol <= 0) || (zielVol >= KA_MAX);
  if (!randErledigt && amEnde) {
    randRest = RESYNC_EXTRA;
    randErledigt = true;
  }
  if (randRest > 0) {
    sendeMitAbstand(zielVol <= 0 ? BEFEHL_LEISER : BEFEHL_LAUTER);   // KaRadio begrenzt, kaVol bleibt
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
