#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include "esp_system.h"

#include "version.h"
#include "wifi_config.h"
#include <ArduinoOTA.h>

// Módulos
#include "logging.h"
#include "wifi_mgr.h"
#include "usbkbd.h"
#include "commands.h"
#include "config.h"

extern Config config;

Preferences prefs;
WiFiUDP rsyslogUdp;
const uint16_t rsyslogPort = 514;
const unsigned long RSYSLOG_RETRY_DELAY = 5000;

// Recursos globais usados pelos módulos
WiFiClient client;
USBHIDKeyboard Keyboard;
bool usbAttached = false;
bool otaEnabled = false;

// Servidor TCP
const uint16_t port = 1234;
WiFiServer tcpServer(port);

// Credenciais WiFi
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Prototipo
void loadConfig();
void saveConfig();

// Carrega configuração
void loadConfig() {
  if (!prefs.begin("kbdcfg", false)) {
    logMsg(F("Failed to initialize Preferences"));
    return;
  }
  config.logToRsyslog = prefs.getBool("logToRsyslog", false);
  config.rsyslogServer = prefs.getString("rsyslogServer", "192.168.5.2");
  config.hostname = prefs.getString("hostname", "esp32kbd");
  config.rsyslogMaxRetries = prefs.getUChar("rsyslogRetries", 3);
  config.keyDelayMs = prefs.getUShort("keyDelayMs", 20);
  prefs.end();

  rsyslog.enabled = config.logToRsyslog;
  rsyslog.failedAttempts = 0;
  rsyslog.lastAttempt = 0;
  rsyslog.temporarilyDisabled = false;
}

// Salva configuração
void saveConfig() {
  if (!prefs.begin("kbdcfg", false)) {
    logMsg(F("Failed to initialize Preferences for saving"));
    return;
  }
  prefs.putBool("logToRsyslog", config.logToRsyslog);
  prefs.putString("rsyslogServer", config.rsyslogServer);
  prefs.putString("hostname", config.hostname);
  prefs.putUShort("keyDelayMs", config.keyDelayMs);
  prefs.end();
  logMsg(F("Configuration saved"));
}

void setup() {
  // Logging (inicial + checagem Serial)
  initLogging();

  // Config
  loadConfig();
  config.bootTime = millis();

  // Hostname
  if (config.hostname.length() > 0) {
    WiFi.setHostname(config.hostname.c_str());
  }

  // USB HID
  USB.begin();
  Keyboard.begin();
  usbAttached = true;

  // WiFi (timeout e rechecagem)
  initWiFi(ssid, password);

  // TCP
  tcpServer.begin();
  logMsg(String(F("Servidor TCP iniciado na porta ")) + String(port));
  logMsg(F("Aguardando conexão do cliente..."));

  // Comandos
  initCommands();

  // Banner inicial
  logMsg(F("=== ESP32 Keyboard Controller ==="));
  logMsg(String(F("Device: ")) + config.hostname);
  logMsg(String(F("Git: commit ")) + GIT_COMMIT + F(" | branch ") + GIT_BRANCH);
  logMsg(String(F("Build: ")) + GIT_DATE);
  logMsg(String(F("Path: ")) + GIT_PATH);
}

void loop() {
  // Rechecagem serial (20s) e WiFi (2 min)
  recheckSerial();
  handleWiFi();

  // TCP: aceitar novo cliente
  if (tcpServer.hasClient()) {
    // Se já houver cliente, desconectar
    if (client && client.connected()) {
      logMsg(F("Cliente existente desconectado."));
      client.stop();
    }
    client = tcpServer.accept();
    logMsg(F("Novo cliente conectado!"));
    logMsg(F("=== Últimas mensagens ==="));
    logMsg(F("=== Digite um comando ou ':cmd help' para ajuda ==="));
  }

  // Watchdog TCP: se cliente desconectou, limpar
  if (client && !client.connected()) {
    logMsg(F("Cliente TCP desconectado."));
    client.stop();
  }

  // Processar comandos do cliente
  if (client && client.connected() && client.available()) {
    String command = client.readStringUntil('\n');
    command.trim();
    processCommand(command);
  }

  // Se OTA habilitado, processar eventos OTA
  if (otaEnabled) {
    ArduinoOTA.handle();
  }

  // Pequena pausa sem bloquear (melhora preempção)
  vTaskDelay(1);
}