// Tastatur: 6 Schalter (Pins 4-9, gegen GND) -> NEC-Signal direkt an ESP32-IR-Pin
// Ausgang wie ein IR-Empfängermodul: Ruhe = HIGH, Impuls = LOW (kein 38-kHz-Träger)
// Kontrolle: Taste n -> LED blinkt n-mal

const int schalterPins[6] = {4, 5, 6, 7, 8, 9};
int letzterZustand[6];

const int ausgangPin = 3;   // -> IR-Pin (P_IR_SIGNAL) des ESP32
const int ledPin = 13;      // eingebaute LED des Pro Mini

const bool LED_FEEDBACK = true;   // nach dem Test auf false setzen

// NEC-Befehle, Adresse 0x00 -> KaRadio-Codes 0xFF00xx
const uint8_t befehle[6] = {0x0C, 0x18, 0x5E, 0x08, 0x1C, 0x5A};

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

void blinke(int anzahl) {
  for (int n = 0; n < anzahl; n++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(150);
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
}

void loop() {
  for (int i = 0; i < 6; i++) {
    int aktuell = digitalRead(schalterPins[i]);

    if (aktuell != letzterZustand[i]) {
      delay(50);                       // Entprellen
      aktuell = digitalRead(schalterPins[i]);

      // nur senden, wenn der Kontakt neu geschlossen wurde
      if (aktuell == LOW && letzterZustand[i] == HIGH) {
        sendeNEC(0x00, befehle[i]);
        if (LED_FEEDBACK) blinke(i + 1);   // Taste 1 = 1x, ..., Taste 6 = 6x
      }
      letzterZustand[i] = aktuell;
    }
  }
}
