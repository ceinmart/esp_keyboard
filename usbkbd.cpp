#include "usbkbd.h"
#include <USBHIDKeyboard.h>
#include "logging.h"
#include "config.h"

extern USBHIDKeyboard Keyboard;
extern bool usbAttached;

void pressAndRelease(uint8_t k) {
  if (!usbAttached) {
    logMsg(F("USB HID não anexado — pressAndRelease ignorado."));
    return;
  }
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.press(k);
  vTaskDelay(pdMS_TO_TICKS(5));
  Keyboard.releaseAll();
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

void handlePressCommand(String keyStr) {
  if (!usbAttached) {
    logMsg(F("USB HID não anexado — comando :press ignorado."));
    return;
  }
  keyStr.trim();
  String s = keyStr;
  s.toUpperCase();

  bool pressCtrl = false, pressShift = false, pressAlt = false, pressWin = false;
  String main;
  int p;
  while ((p = s.indexOf('+')) >= 0) {
    String part = s.substring(0, p);
    part.trim();
    if (part == F("CTRL") || part == F("CONTROL")) pressCtrl = true;
    else if (part == F("SHIFT")) pressShift = true;
    else if (part == F("ALT")) pressAlt = true;
    else if (part == F("WIN") || part == F("GUI")) pressWin = true;
    s = s.substring(p + 1);
  }
  s.trim();
  main = s;

  uint8_t code = 0;
  if (main.length() == 1) {
    code = main.charAt(0);
  } else if (main == F("ENTER")) code = KEY_RETURN;
  else if (main == F("TAB")) code = KEY_TAB;
  else if (main == F("ESC") || main == F("ESCAPE")) code = KEY_ESC;
  else if (main == F("BACKSPACE")) code = KEY_BACKSPACE;
  else if (main == F("DELETE")) code = KEY_DELETE;
  else if (main == F("SPACE")) code = ' ';
  else if (main == F("UP") || main == F("UPARROW")) code = KEY_UP_ARROW;
  else if (main == F("DOWN") || main == F("DOWNARROW")) code = KEY_DOWN_ARROW;
  else if (main == F("LEFT") || main == F("LEFTARROW")) code = KEY_LEFT_ARROW;
  else if (main == F("RIGHT") || main == F("RIGHTARROW")) code = KEY_RIGHT_ARROW;
  else if (main.startsWith(F("F")) && main.length() <= 3) {
    int fn = main.substring(1).toInt();
    if (fn >= 1 && fn <= 12) code = KEY_F1 + (fn - 1);
  }

  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  if (pressCtrl)  Keyboard.press(KEY_LEFT_CTRL);
  if (pressShift) Keyboard.press(KEY_LEFT_SHIFT);
  if (pressAlt)   Keyboard.press(KEY_LEFT_ALT);
  if (pressWin)   Keyboard.press(KEY_LEFT_GUI);

  if (code) Keyboard.press(code);
  vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
  Keyboard.releaseAll();
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
  logMsg(String(F("Pressionado: ")) + keyStr);
}

void processAndType(const String &txt) {
  if (!usbAttached) {
    logMsg(F("USB HID não anexado — comando de digitação ignorado."));
    return;
  }

  for (size_t i = 0; i < txt.length();) {
    char c = txt.charAt(i);
    if (c == '\\' && (i + 1) < txt.length()) {
      char next = txt.charAt(i + 1);
      if (next == 'n') {
        pressAndRelease(KEY_RETURN);
        logMsg(F("Pressionado: ENTER"));
        vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
        i += 2;
        continue;
      } else if (next == '\\') {
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.print('\\');
  vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
        i += 2;
        continue;
      } else {
        if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
        Keyboard.print(next);
        vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
        if (keyboardMutex) xSemaphoreGive(keyboardMutex);
        i += 2;
        continue;
      }
    }
    if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
    Keyboard.print(c);
    vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
    if (keyboardMutex) xSemaphoreGive(keyboardMutex);
    i++;
  }
}