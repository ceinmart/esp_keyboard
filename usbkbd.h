//
// Arquivo: usbkbd.h
// Módulo responsável pela interface com o teclado USB HID
// Inclui funções seguras para operações do teclado e processamento de comandos
//
#pragma once
#include <Arduino.h>
#include "config.h"

// Funções principais de manipulação do teclado
// A inicialização USB HID é feita em esp32_keyboard.ino (USB.begin/Keyboard.begin)

// Funções de alto nível para comandos do teclado
void pressAndRelease(uint8_t k);                  // Pressiona e solta uma tecla
void handlePressCommand(String keyStr);           // Processa comando :press (teclas com modificadores)
void processAndType(const String &txt);           // Processa e digita texto com caracteres especiais

// Wrappers seguros que centralizam o controle de mutex para operações USB
// Estas funções garantem acesso sincronizado ao teclado entre tasks
void safeKeyboardBegin();                         // Inicializa o teclado de forma segura
void safeKeyboardEnd();                           // Finaliza o teclado de forma segura
void safeKeyboardPrint(const String &s);          // Imprime texto de forma segura

// Ações de teclado de alto nível com proteção de mutex
void safeKeyboardTap(uint8_t k,                   // Pressiona e solta (tap) uma tecla
                     uint16_t holdMs = 0);        // holdMs: tempo de pressionamento
void safeKeyboardHold(uint8_t k);                 // Mantém uma tecla pressionada
void safeKeyboardRelease(uint8_t k);              // Solta uma tecla que estava pressionada