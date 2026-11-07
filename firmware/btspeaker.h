/*
 * btspeaker.h - Bluetooth-Lautsprecher (A2DP-Sink) für KaRadio32 (dansoft-ch/KaRadio)
 */
#ifndef BTSPEAKER_H
#define BTSPEAKER_H

#include <stdbool.h>

#define BT_DEVICE_NAME "Biennophone"

// Radio stoppen, Bluetooth sichtbar machen, Ton vom Handy ausgeben.
void btSpeakerStart(void);
// Handy trennen, Bluetooth unsichtbar machen. Das Radio startet der Aufrufer.
void btSpeakerStop(void);
// true, solange der Bluetooth-Betrieb aktiv ist (Radio ist dann gesperrt).
bool btSpeakerActive(void);

#endif
