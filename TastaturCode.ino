#include <IRremote.hpp>

// Definition der Pins für die 6 Schalter
// Code ist zu verifizieren ; 2026.10.23 Daniel
const int schalterPins[6] = {2, 3, 4, 5, 6, 7}; 
int letzterZustand[6] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};

// Die NEC-Infrarot-Codes für die Tasten 1 bis 6 (Beispiel-Codes)
const uint32_t irCodes[6] = {
  0xFF30CF, // Taste 1
  0xFF18E7, // Taste 2
  0xFF7A85, // Taste 3
  0xFF10EF, // Taste 4
  0xFF38C7, // Taste 5
  0xFF5AA5  // Taste 6
};

const int irSendePin = 9; // Dieses Kabel geht zum IR-Pin des ESP32

void setup() {
  IrSender.begin(irSendePin); // Initialisiere den IR-Sender
  
  for (int i = 0; i < 6; i++) {
    pinMode(schalterPins[i], INPUT_PULLUP); // Interne Widerstände aktivieren
    letzterZustand[i] = digitalRead(schalterPins[i]);
  }
}

void loop() {
  for (int i = 0; i < 6; i++) {
    int aktuellerZustand = digitalRead(schalterPins[i]);
    
    // Wenn der Schalter umgelegt wurde (egal ob ein- oder ausgeschaltet)
    if (aktuellerZustand != letzterZustand[i]) {
      delay(50); // Entprellen des Schalters
      
      // Sende den NEC-Code an das Webradio
      IrSender.sendNEC(0x00, irCodes[i], 0); 
      
      letzterZustand[i] = aktuellerZustand; // Zustand merken
    }
  }
}
