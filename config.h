// config.h
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>

// Constante global
inline constexpr int TCP_BUFFER_SIZE = 50;

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
