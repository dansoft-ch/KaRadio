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
// Radio-Betrieb ab Start: reservierten BT-Speicher (ca. 56 KB intern) ans Radio
// abgeben. Bluetooth geht danach erst nach einem Neustart wieder.
void btSpeakerReleaseMemory(void);
// false, wenn der Speicher abgegeben wurde (Bluetooth braucht dann einen Neustart).
bool btSpeakerAvailable(void);

#endif
