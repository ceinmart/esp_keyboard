#include "wifi_mgr.h"
#include "logging.h"


static const unsigned long WIFI_CONNECT_TIMEOUT = 120000; // 2 minutos
static const unsigned long WIFI_RECHECK_INTERVAL = 120000; // 2 minutos
static unsigned long lastWiFiCheck = 0;

void initWiFi(const char* ssid, const char* password) {
  logMsg(String(F("Conectando ao WiFi ")) + ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
    vTaskDelay(pdMS_TO_TICKS(500));
    if (serialEnabled) Serial.print(F("."));
  }

  if (WiFi.status() == WL_CONNECTED) {
    logMsg(F("WiFi conectado!"));
    logMsg(String(F("Endereço IP: ")) + WiFi.localIP().toString());
    // Flush any buffered rsyslog messages queued before WiFi was ready
    flushBufferedRsyslog();
  } else {
    logMsg(F("Falha ao conectar WiFi (timeout de 2 minutos)."));
  }
}

void handleWiFi() {
  if (millis() - lastWiFiCheck >= WIFI_RECHECK_INTERVAL) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      logMsg(F("WiFi desconectado, tentando reconectar..."));
      WiFi.reconnect();

      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (serialEnabled) Serial.print(F("."));
      }

      if (WiFi.status() == WL_CONNECTED) {
        logMsg(F("WiFi reconectado com sucesso!"));
        logMsg(String(F("Endereço IP: ")) + WiFi.localIP().toString());
        // Flush messages buffered while offline
        flushBufferedRsyslog();
      } else {
        logMsg(F("Falha ao reconectar WiFi (timeout de 2 minutos)."));
      }
    }
  }
}