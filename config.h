// config.h
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
// FreeRTOS semaphore type used to protect USB HID access
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Constante global
static constexpr int TCP_BUFFER_SIZE = 50;

// Structs
struct OutputBuffer {
  String lines[TCP_BUFFER_SIZE];
  int start;
  int count;
};

struct Config {
  bool logToRsyslog;
  String rsyslogServer;
  String hostname;
  unsigned long bootTime;
  uint8_t rsyslogMaxRetries;
  uint16_t keyDelayMs;
};

struct RsyslogState {
  bool enabled;
  uint8_t failedAttempts;
  unsigned long lastAttempt;
  bool temporarilyDisabled;
};

// Variáveis globais (apenas declarações)
extern OutputBuffer outputBuffer;
extern Config config;
extern RsyslogState rsyslog;
// Mutex to protect Keyboard/USB HID operations across tasks
extern SemaphoreHandle_t keyboardMutex;
