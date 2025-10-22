#include "logging.h"
#include <WiFiUdp.h>
#include <WiFi.h>

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
  // Only attempt to send to rsyslog if configured and WiFi is connected.
  // Sending to rsyslog may perform DNS resolution which requires the TCP/IP
  // stack; avoid calling it before WiFi is up to prevent calling into
  // lwIP/tcpip APIs from setup context and triggering asserts.
  if (config.logToRsyslog && WiFi.status() == WL_CONNECTED) {
    sendToRsyslog(msg);
  }
}

void sendToRsyslog(String msg) {
  // Avoid attempting to send if feature disabled or temporarily disabled.
  if (!config.logToRsyslog || rsyslog.temporarilyDisabled) return;

  // Ensure WiFi is connected before attempting DNS/UDP operations.
  if (WiFi.status() != WL_CONNECTED) {
    // Skip sending — caller (logMsg) should have already checked, but
    // double-check here to be safe.
    return;
  }

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
      // Avoid calling logMsg here because that would attempt to resend to
      // rsyslog and may re-enter this error path. Print directly to Serial
      // and client instead.
      if (serialEnabled) {
        Serial.println(String(F("Rsyslog disabled after ")) + rsyslog.failedAttempts +
                       F(" failed attempts: ") + error);
      }
      if (client && client.connected()) {
        client.println(String(F("Rsyslog disabled after ")) + rsyslog.failedAttempts +
                       F(" failed attempts: ") + error);
      }
      rsyslog.temporarilyDisabled = true;
    }
  } else {
    // Print error status to serial/client but avoid calling logMsg to
    // prevent recursion into sendToRsyslog.
    if (serialEnabled) {
      Serial.println(String(F("Rsyslog error (attempt ")) + rsyslog.failedAttempts + F("/") +
                     config.rsyslogMaxRetries + F(": ") + error);
    }
    if (client && client.connected()) {
      client.println(String(F("Rsyslog error (attempt ")) + rsyslog.failedAttempts + F("/") +
                     config.rsyslogMaxRetries + F(": ") + error);
    }
  }
}