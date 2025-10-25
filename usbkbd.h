#pragma once
#include <Arduino.h>
#include "config.h"


// Inicialização USB HID já é feita em main.ino (USB.begin/Keyboard.begin)
// Aqui mantemos utilitários de teclado e digitação.

void pressAndRelease(uint8_t k);
void handlePressCommand(String keyStr);
void processAndType(const String &txt);

// Safe wrappers to centralize mutex handling for Keyboard/USB operations
void safeKeyboardBegin();
void safeKeyboardEnd();
void safeKeyboardPrint(const String &s);

// Higher-level keyboard actions
// Tap: press then release (default behavior)
void safeKeyboardTap(uint8_t k, uint16_t holdMs = 0);
// Hold: press the key and keep it held until safeKeyboardRelease is called
void safeKeyboardHold(uint8_t k);
// Release a held key
void safeKeyboardRelease(uint8_t k);