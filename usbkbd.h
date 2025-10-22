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
void safeKeyboardPress(uint8_t k);