#include "logging.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include <stdio.h>

// Helper: try to parse dotted IPv4 string into IPAddress. Returns true on success.
static bool parseIPv4(const String &s, IPAddress &out) {
  unsigned int a = 0, b = 0, c = 0, d = 0;
  int matched = sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
  if (matched == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255) {
    out = IPAddress((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
  }
  return false;
}

bool serialEnabled = false;
static unsigned long lastSerialCheck = 0;
const unsigned long SERIAL_RECHECK_INTERVAL = 20000; // 20s
// Diagnostic counter for rsyslog sends
static uint32_t rsyslogSendCounter = 0;
static String rsyslogLastMsg = String();
static unsigned long rsyslogLastSendTime = 0;
// Sequence counter and debug flag to help diagnose server-side duplication
static uint32_t rsyslogSeq = 0;
static bool rsyslogDebug = false;

void initLogging() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);  // Enable debug output
  
  unsigned long start = millis();
  // Wait up to 3 seconds for serial connection
  while (millis() - start < 3000) {
    if (Serial) {
      serialEnabled = true;
      break;
    }
    delay(100);
  }

  if (serialEnabled) {
    Serial.println(F("Serial conectado, logs habilitados."));
  } else {
    // If the CDC serial is not available, still print to the ROM UART
    Serial.println(F("Serial não detectado no timeout inicial."));
    printf("Serial não detectado no timeout inicial.\n");
  }
  
  // Force flush any pending data
  Serial.flush();
}

void recheckSerial() {
  if (millis() - lastSerialCheck >= SERIAL_RECHECK_INTERVAL) {
    lastSerialCheck = millis();
    
    // Check if Serial is actually available
    if (Serial) {
      if (!serialEnabled) {
        serialEnabled = true;
        Serial.println(F("Serial conectado após boot, logs habilitados."));
        Serial.flush();
      }
    } else {
      if (serialEnabled) {
        serialEnabled = false;
        // Don't try to print since serial is not available
      }
    }
  }
}

void logMsg(const String &msg) {
  // Print to CDC serial when available, otherwise print to ROM UART
  if (serialEnabled) {
    Serial.println(msg);
  } else {
    // Fallback to ROM UART (visible on the same COM that shows boot logs)
    printf("%s\n", msg.c_str());
  }

  if (client && client.connected()) {
    client.println(msg);
  }

  // Only send to rsyslog if enabled and WiFi is connected
  if (config.logToRsyslog && WiFi.status() == WL_CONNECTED) {
    sendToRsyslog(msg);
  }
}

void sendToRsyslog(String msg) {
  // Exit early if logging disabled or in failed state
  if (!rsyslog.enabled || !config.logToRsyslog || rsyslog.temporarilyDisabled) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (rsyslog.failedAttempts > 0 &&
      (millis() - rsyslog.lastAttempt) >= RSYSLOG_RETRY_DELAY) {
    rsyslog.failedAttempts = 0;
  }

  rsyslog.lastAttempt = millis();

  // Prepare outgoing message (may add seq prefix in debug mode)
  String out = msg;
  if (rsyslogDebug) {
    rsyslogSeq++;
    out = String(F("[SEQ:")) + String(rsyslogSeq) + F("] ") + out;
  }
  // Add newline if not present for proper syslog format
  if (!out.endsWith("\n")) {
    out += "\n";
  }

  // Prefer sending directly to an IPAddress if the configured server is a dotted IPv4
  IPAddress ip;
  bool usedIp = false;
  if (parseIPv4(config.rsyslogServer, ip)) {
    usedIp = true;
    if (!rsyslogUdp.beginPacket(ip, rsyslogPort)) {
      if (serialEnabled) Serial.printf("beginPacket(IP %s:%d) failed\n", config.rsyslogServer.c_str(), rsyslogPort);
      else printf("beginPacket(IP %s:%d) failed\n", config.rsyslogServer.c_str(), rsyslogPort);
      handleRsyslogError("Failed to begin UDP packet (ip)");
      return;
    }
  } else {
    if (!rsyslogUdp.beginPacket(config.rsyslogServer.c_str(), rsyslogPort)) {
      if (serialEnabled) Serial.printf("beginPacket(host %s:%d) failed\n", config.rsyslogServer.c_str(), rsyslogPort);
      else printf("beginPacket(host %s:%d) failed\n", config.rsyslogServer.c_str(), rsyslogPort);
      handleRsyslogError("Failed to begin UDP packet (host)");
      return;
    }
  }

  rsyslogUdp.write((const uint8_t *)out.c_str(), out.length());

  // Diagnostic: increment counter and detect quick duplicates (only when debug enabled)
  if (rsyslogDebug) {
    rsyslogSendCounter++;
    unsigned long now = millis();
    bool quickDup = (rsyslogLastMsg == out && (now - rsyslogLastSendTime) < 50);
    rsyslogLastMsg = out;
    rsyslogLastSendTime = now;
    if (serialEnabled) {
      Serial.printf("RSYSLOG SEND #%u %s %s", rsyslogSendCounter, config.rsyslogServer.c_str(), out.c_str());
      if (quickDup) Serial.println(F("[QUICK DUP DETECTED]"));
    } else {
      printf("RSYSLOG SEND #%u %s %s", rsyslogSendCounter, config.rsyslogServer.c_str(), out.c_str());
      if (quickDup) printf("[QUICK DUP DETECTED]\n");
    }
  }

  if (!rsyslogUdp.endPacket()) {
    // endPacket can fail; provide diagnostic including destination form
    if (serialEnabled) {
      if (usedIp) Serial.printf("endPacket failed to %s (ip)\n", config.rsyslogServer.c_str());
      else Serial.printf("endPacket failed to %s (host)\n", config.rsyslogServer.c_str());
    } else {
      if (usedIp) printf("endPacket failed to %s (ip)\n", config.rsyslogServer.c_str());
      else printf("endPacket failed to %s (host)\n", config.rsyslogServer.c_str());
    }
    handleRsyslogError("Failed to send UDP packet");
    return;
  }

  if (rsyslog.failedAttempts > 0) {
    rsyslog.failedAttempts = 0;
    // Print restoration notice directly to avoid re-entering sendToRsyslog via logMsg
    if (serialEnabled) Serial.println(F("Rsyslog connection restored"));
    else printf("Rsyslog connection restored\n");
    if (client && client.connected()) client.println(F("Rsyslog connection restored"));
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

// Re-enable rsyslog after administrator intervention or on demand
void enableRsyslog() {
  // Reset UDP connection first
  rsyslogUdp.stop();
  delay(100); // Small delay to let UDP state settle
  
  // Reset state
  rsyslog.failedAttempts = 0;
  rsyslog.temporarilyDisabled = false;
  rsyslog.enabled = config.logToRsyslog;
  rsyslog.lastAttempt = millis();
  
  // Try to send a test message
  String testMsg = String(F("Rsyslog re-enabled on ")) + config.hostname + F(" at ") + String(millis());
  
  // Only emit the explicit re-enable/test diagnostics when debug mode is active
  if (rsyslogDebug) {
    if (serialEnabled) {
      Serial.println(F("Rsyslog explicitly re-enabled"));
      Serial.println(F("Sending test message..."));
    } else {
      printf("Rsyslog explicitly re-enabled\n");
      printf("Sending test message...\n");
    }
    if (client && client.connected()) {
      client.println(F("Rsyslog explicitly re-enabled"));
      client.println(F("Sending test message..."));
    }
  }
  
  // This will go through sendToRsyslog which has all the proper checks
  logMsg(testMsg);
}

void setRsyslogDebug(bool enabled) {
  rsyslogDebug = enabled;
  // Announce the change via standard logging (so it follows normal log flow)
  logMsg(String(F("Rsyslog debug ")) + (enabled ? F("ENABLED") : F("DISABLED")));
}