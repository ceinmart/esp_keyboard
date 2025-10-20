#pragma once
#include <Arduino.h>
#include <config.h>

extern Config config;

// Inicialização USB HID já é feita em main.ino (USB.begin/Keyboard.begin)
// Aqui mantemos utilitários de teclado e digitação.

void pressAndRelease(uint8_t k);
void handlePressCommand(String keyStr);
void processAndType(const String &txt);