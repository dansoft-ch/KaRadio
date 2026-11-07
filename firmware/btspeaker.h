/*
 * btspeaker.h - Bluetooth-Lautsprecher (A2DP-Sink) für KaRadio32 (dansoft-ch/KaRadio)
 * v2: Bluetooth immer an, automatische Umschaltung Radio <-> Handy.
 */
#ifndef BTSPEAKER_H
#define BTSPEAKER_H

#include <stdbool.h>

#define BT_DEVICE_NAME "Biennophone"

// Bluetooth-Stack starten und sichtbar machen (einmal beim Start).
void btSpeakerInit(void);
// Umschaltung Radio <-> Bluetooth, regelmässig aus einem Task aufrufen.
void btSpeakerPoll(void);
// Sendertaste gedrückt: Handy trennen, Bluetooth-Ton aus (Sender spielt der Aufrufer).
void btSpeakerToRadio(void);
// true, solange der Bluetooth-Ton läuft (Radio ist dann gesperrt).
bool btSpeakerActive(void);

#endif
