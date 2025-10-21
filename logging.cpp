#include "logging.h"
#include <WiFiUdp.h>

bool serialEnabled = false;
static unsigned long lastSerialCheck = 0;
const unsigned long SERIAL_RECHECK_INTERVAL = 20000; // 20s

void initLogging() {
  Serial.begin(115200);

  unsigned long start = millis();
  while (millis() - start < 2000) {
    if (Serial && Serial.availableForWrite() > 0) {
      serialEnabled = true;
      break;
    }
    delay(10);
  }

  if (serialEnabled) {
    Serial.println(F("Serial conectado, logs habilitados."));
  }
}

void recheckSerial() {
  if (millis() - lastSerialCheck >= 20000) {
    lastSerialCheck = millis();
    if (Serial && Serial.availableForWrite() > 0) {
      if (!serialEnabled) {
        Serial.println(F("Serial conectado após boot, logs habilitados."));
      }
      serialEnabled = true;
    }
  }
}

void logMsg(const String &msg) {
  if (serialEnabled) {
    Serial.println(msg);
  }
  if (client && client.connected()) {
    client.println(msg);
  }
  if (config.logToRsyslog) {
    sendToRsyslog(msg);
  }
}

void sendToRsyslog(String msg) {
  if (!config.logToRsyslog || rsyslog.temporarilyDisabled) return;

  if (rsyslog.failedAttempts > 0 &&
      (millis() - rsyslog.lastAttempt) >= RSYSLOG_RETRY_DELAY) {
    rsyslog.failedAttempts = 0;
  }

  rsyslog.lastAttempt = millis();
  if (!rsyslogUdp.beginPacket(config.rsyslogServer.c_str(), rsyslogPort)) {
    handleRsyslogError("Failed to begin UDP packet");
    return;
  }

  rsyslogUdp.write((const uint8_t *)msg.c_str(), msg.length());

  if (!rsyslogUdp.endPacket()) {
    handleRsyslogError("Failed to send UDP packet");
    return;
  }

  if (rsyslog.failedAttempts > 0) {
    rsyslog.failedAttempts = 0;
    logMsg(F("Rsyslog connection restored"));
  }
}

void handleRsyslogError(const char *error) {
  rsyslog.failedAttempts++;
  if (rsyslog.failedAttempts >= config.rsyslogMaxRetries) {
    if (!rsyslog.temporarilyDisabled) {
      logMsg(String(F("Rsyslog disabled after ")) + rsyslog.failedAttempts +
             F(" failed attempts: ") + error);
      rsyslog.temporarilyDisabled = true;
    }
  } else {
    logMsg(String(F("Rsyslog error (attempt ")) + rsyslog.failedAttempts + "/" +
           config.rsyslogMaxRetries + F("): ") + error);
  }
}