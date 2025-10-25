//
// Arquivo: esp32_keyboard.ino
// Teclado USB HID WiFi com ESP32-S3
// Permite controlar um teclado USB via comandos TCP
// 
// Características principais:
// - Emulação de teclado USB usando USBHIDKeyboard
// - Controle via TCP (porta 1234)
// - Suporte a OTA (Over The Air updates)
// - Logging remoto via rsyslog
// - Configurações persistentes
// - Comandos para controle de teclas e configuração
//
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include "esp_system.h"

// Headers de versão e configuração
#include "version.h"
#include "wifi_config.h"
#include <ArduinoOTA.h>

// Módulos do sistema
#include "logging.h"    // Sistema de logging
#include "wifi_mgr.h"   // Gerenciamento WiFi
#include "usbkbd.h"     // Interface USB HID
#include "commands.h"    // Processamento de comandos
#include "config.h"      // Configurações

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
  // Application start marker (always printed to ROM UART)
  printf("APP START: esp32_keyboard starting...\n");
  // Initialize USB stack first
  USB.begin();
  delay(100);  // Give USB time to initialize
  
  // Initialize Serial and logging
  initLogging();

  // Config
  loadConfig();
  config.bootTime = millis();

  // Hostname
  if (config.hostname.length() > 0) {
    WiFi.setHostname(config.hostname.c_str());
  }

  // USB HID
  // Ensure the keyboard mutex exists before any Keyboard calls
  if (keyboardMutex == NULL) {
    keyboardMutex = xSemaphoreCreateMutex();
    if (keyboardMutex == NULL) {
      // If mutex creation fails, log but continue; code paths check for NULL
      logMsg(F("Falha ao criar keyboardMutex — operações de teclado ficarão sem proteção."));
    } else {
      logMsg(F("keyboardMutex criado com sucesso."));
    }
  }

  // Initialize keyboard after USB
  safeKeyboardBegin();
  usbAttached = true;
  
  // Force Serial reinit after USB setup
  if (!Serial) {
    Serial.end();
    delay(100);
    Serial.begin(115200);
    delay(100);
  }

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
