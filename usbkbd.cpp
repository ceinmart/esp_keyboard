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
  safeKeyboardTap(k, config.keyDelayMs);
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

  // Hold modifiers, tap the main key, then release modifiers
  // Hold modifiers
  // Hold modifiers with proper delay between each
  if (pressCtrl) {
    safeKeyboardHold(KEY_LEFT_CTRL);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressShift) {
    safeKeyboardHold(KEY_LEFT_SHIFT);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressAlt) {
    safeKeyboardHold(KEY_LEFT_ALT);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressWin) {
    safeKeyboardHold(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Add small delay after modifiers before main key
  if (pressCtrl || pressShift || pressAlt || pressWin) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  // tap the main key
  if (code) {
    safeKeyboardTap(code, config.keyDelayMs);
  } else if (pressWin) {
    // Special case: If just WIN key pressed with no main key
    vTaskDelay(pdMS_TO_TICKS(config.keyDelayMs));
  }

  // Add small delay before releasing modifiers
  vTaskDelay(pdMS_TO_TICKS(20));

  // release modifiers in reverse order with small delays
  if (pressWin) {
    safeKeyboardRelease(KEY_LEFT_GUI);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressAlt) {
    safeKeyboardRelease(KEY_LEFT_ALT);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressShift) {
    safeKeyboardRelease(KEY_LEFT_SHIFT);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (pressCtrl) {
    safeKeyboardRelease(KEY_LEFT_CTRL);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
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
  safeKeyboardPrint("\\");
        i += 2;
        continue;
      } else {
        safeKeyboardPrint(String(next));
        i += 2;
        continue;
      }
    }
    safeKeyboardPrint(String(c));
    i++;
  }
}

// Safe wrappers implementation
void safeKeyboardBegin() {
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.begin();
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

void safeKeyboardEnd() {
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.end();
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

void safeKeyboardPrint(const String &s) {
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.print(s);
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

void safeKeyboardPress(uint8_t k) {
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.press(k);
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

// Implementation of higher-level safe keyboard actions
void safeKeyboardTap(uint8_t k, uint16_t holdMs) {
  if (!usbAttached) return;
  uint16_t ms = holdMs;
  if (ms < 20) ms = 20;
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.press(k);
  vTaskDelay(pdMS_TO_TICKS(ms));
#if defined(KEYBOARD_RELEASE)
  Keyboard.release(k);
#else
  Keyboard.releaseAll();
#endif
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
  vTaskDelay(pdMS_TO_TICKS(5));
}

void safeKeyboardHold(uint8_t k) {
  if (!usbAttached) return;
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
  Keyboard.press(k);
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}

void safeKeyboardRelease(uint8_t k) {
  if (!usbAttached) return;
  if (keyboardMutex) xSemaphoreTake(keyboardMutex, pdMS_TO_TICKS(100));
#if defined(KEYBOARD_RELEASE)
  Keyboard.release(k);
#else
  Keyboard.releaseAll();
#endif
  if (keyboardMutex) xSemaphoreGive(keyboardMutex);
}