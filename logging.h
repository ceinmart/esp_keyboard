#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "config.h"

// Forward decl de estruturas globais
struct Config;
struct RsyslogState;

// API de logging
void initLogging();
void recheckSerial();
void logMsg(const String &msg);
void handleRsyslogError(const char *error);
void sendToRsyslog(String msg);
// Rsyslog control
void enableRsyslog();
void setRsyslogDebug(bool enabled);

// Externs globais (definidos em main.ino)
extern bool serialEnabled;
extern WiFiClient client;
extern RsyslogState rsyslog;
extern WiFiUDP rsyslogUdp;
extern const uint16_t rsyslogPort;
extern const unsigned long RSYSLOG_RETRY_DELAY;