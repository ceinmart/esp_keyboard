//TODO: adicionar no codigo tags de versão e data/hora de compilação e revisão do git, exibir estas informações no comando de status.
//TODO OK : adicionar a configuração do wifi no prefs , mantendo os valores atuais como default.
//TODO: como fazer update via OTA
//TODO: opcão de salvar templates de escritas a serem enviadas e então chamar o template via comando

//TODO:OK validar porque as configurações não estão sendo salvas no Preferences ,quando reinicia o esp32 o rsyslogd não reconecta.
//TODO:OK adicinar no comando status os valores salvos no Preferences
//TODO:OK adicionar no status o uptime.
//TODO:OK ajustar a syntaxde de comando para de ^CMD: para :cmd<espaço> , onde não usa : como separador e sim o espaço e não ter case sensitive
//TODO:OK adicionar suporte para reboot via :cmd reboot
//TODO:OK adicionar controle na conexão do rsyslogd , caso falhe , fazer X tentantivas e depois desabilitar o log
//TODO:OK adicionar suporte para exibir output via tcp quando conectado via "nc"
//TODO:OK adicionar suporte para mqtt e integração no Home assistant.
//TODO:OK suporte para ajustar o hostname via configuração CMD

#include <WiFi.h>
#include <WiFiClient.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include "esp_system.h"
// OTA
#include <ArduinoOTA.h>
// Configuration structure
// Circular buffer for TCP output
const int TCP_BUFFER_SIZE = 50;
struct OutputBuffer {
    String lines[TCP_BUFFER_SIZE];
    int start;
    int count;
    
    OutputBuffer() : start(0), count(0) {}
    
    void add(const String &line) {
        int pos = (start + count) % TCP_BUFFER_SIZE;
        if (count < TCP_BUFFER_SIZE) {
            lines[pos] = line;
            count++;
        } else {
            lines[pos] = line;
            start = (start + 1) % TCP_BUFFER_SIZE;
        }
    }
    
    void clear() {
        start = 0;
        count = 0;
    }
    
    void sendTo(WiFiClient &client) {
        for (int i = 0; i < count; i++) {
            int pos = (start + i) % TCP_BUFFER_SIZE;
            client.println(lines[pos]);
        }
    }
} outputBuffer;

struct Config {
  bool logToRsyslog;
  String rsyslogServer;
  String hostname;
  unsigned long bootTime;
  uint8_t rsyslogMaxRetries;  // Maximum number of retries before disabling
  uint16_t keyDelayMs;        // Delay entre teclas (ms)
    
    // MQTT Configuration
  // (no MQTT in this branch)
} config;

// WiFi reconnect helpers
unsigned long lastWifiReconnectAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000; // try every 10s
bool wifiWasConnected = false;

struct RsyslogState {
    bool enabled;
    uint8_t failedAttempts;
    unsigned long lastAttempt;
    bool temporarilyDisabled;
} rsyslog;

Preferences prefs;
const uint16_t rsyslogPort = 514;
const unsigned long RSYSLOG_RETRY_DELAY = 5000;  // 5 seconds between retries
WiFiUDP rsyslogUdp;
bool usbAttached = false;

// Load configuration from Preferences
// MQTT support removed in this variant

void loadConfig() {
    if (!prefs.begin("kbdcfg", false)) {
        logMsg("Failed to initialize Preferences");
        return;
    }
  config.logToRsyslog = prefs.getBool("logToRsyslog", false);
  config.rsyslogServer = prefs.getString("rsyslogServer", "192.168.5.2");
  config.hostname = prefs.getString("hostname", "esp32kbd");
  config.rsyslogMaxRetries = prefs.getUChar("rsyslogRetries", 3);
  config.keyDelayMs = prefs.getUShort("keyDelayMs", 20);
    
  // No MQTT configuration in this build
    
    prefs.end();
    
    // Initialize rsyslog state
    rsyslog.enabled = config.logToRsyslog;
    rsyslog.failedAttempts = 0;
    rsyslog.lastAttempt = 0;
    rsyslog.temporarilyDisabled = false;
}

// Save configuration to Preferences
void saveConfig() {
    if (!prefs.begin("kbdcfg", false)) {
        logMsg("Failed to initialize Preferences for saving");
        return;
    }
  prefs.putBool("logToRsyslog", config.logToRsyslog);
  prefs.putString("rsyslogServer", config.rsyslogServer);
  prefs.putString("hostname", config.hostname);
  prefs.putUShort("keyDelayMs", config.keyDelayMs);
    
  // No MQTT configuration to save in this build
    
    prefs.end();
    logMsg("Configuration saved");
}

void sendToRsyslog(String msg) {
  if (!config.logToRsyslog || rsyslog.temporarilyDisabled) return;
  
  // Check if we should retry after temporary disable
  if (rsyslog.failedAttempts > 0 && 
      (millis() - rsyslog.lastAttempt) >= RSYSLOG_RETRY_DELAY) {
    rsyslog.failedAttempts = 0;  // Reset counter after delay
  }
  
  // Attempt to send
  rsyslog.lastAttempt = millis();
  if (!rsyslogUdp.beginPacket(config.rsyslogServer.c_str(), rsyslogPort)) {
    handleRsyslogError("Failed to begin UDP packet");
    return;
  }
  
  rsyslogUdp.write((const uint8_t*)msg.c_str(), msg.length());
  
  if (!rsyslogUdp.endPacket()) {
    handleRsyslogError("Failed to send UDP packet");
    return;
  }
  
  // Success - reset failure counter
  if (rsyslog.failedAttempts > 0) {
    rsyslog.failedAttempts = 0;
    Serial.println("Rsyslog connection restored");
  }
}

// Unified logging: prints to Serial and, if enabled, to rsyslog
void handleRsyslogError(const char* error) {
  rsyslog.failedAttempts++;
  
  if (rsyslog.failedAttempts >= config.rsyslogMaxRetries) {
    if (!rsyslog.temporarilyDisabled) {
      Serial.printf("Rsyslog disabled after %d failed attempts: %s\n", 
                   rsyslog.failedAttempts, error);
      rsyslog.temporarilyDisabled = true;
    }
  } else {
    Serial.printf("Rsyslog error (attempt %d/%d): %s\n", 
                 rsyslog.failedAttempts, config.rsyslogMaxRetries, error);
  }
}

void logMsg(const String &msg) {
  Serial.println(msg);
  outputBuffer.add(msg);  // Store in circular buffer for TCP clients
  if (config.logToRsyslog) sendToRsyslog(msg);
}
USBHIDKeyboard Keyboard;
// OTA control flag
bool otaEnabled = false;

// Substitua pelas suas credenciais de WiFi
const char* ssid = "dmartins";
const char* password = "192@dmartins";

// Porta para o servidor TCP
const uint16_t port = 1234;
WiFiServer tcpServer(port);
WiFiClient client;

// Helper: press a key and release
void pressAndRelease(uint8_t k) {
  Keyboard.press(k);
  Keyboard.releaseAll();
}

// Convert esp reset reason to string
const char* resetReasonToString(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

// Robust PRESS handler: supports modifiers separated by '+' (e.g. ctrl+alt+del)
void handlePressCommand(String keyStr) {
  keyStr.trim();
  String s = keyStr;
  s.toUpperCase();

  bool pressCtrl=false, pressShift=false, pressAlt=false, pressWin=false;
  String main;
  // split by +
  int p;
  while ((p = s.indexOf('+')) >= 0) {
    String part = s.substring(0, p);
    part.trim();
    if (part == "CTRL" || part == "CONTROL") pressCtrl = true;
    else if (part == "SHIFT") pressShift = true;
    else if (part == "ALT") pressAlt = true;
    else if (part == "WIN" || part == "GUI") pressWin = true;
    // else could be other modifiers
    s = s.substring(p+1);
  }
  s.trim();
  main = s;

  uint8_t code = 0;
  if (main.length() == 1) {
    code = main.charAt(0);
  } else if (main == "ENTER") code = KEY_RETURN;
  else if (main == "TAB") code = KEY_TAB;
  else if (main == "ESC" || main == "ESCAPE") code = KEY_ESC;
  else if (main == "BACKSPACE") code = KEY_BACKSPACE;
  else if (main == "DELETE") code = KEY_DELETE;
  else if (main == "SPACE") code = ' ';
  else if (main == "UP" || main == "UPARROW") code = KEY_UP_ARROW;
  else if (main == "DOWN" || main == "DOWNARROW") code = KEY_DOWN_ARROW;
  else if (main == "LEFT" || main == "LEFTARROW") code = KEY_LEFT_ARROW;
  else if (main == "RIGHT" || main == "RIGHTARROW") code = KEY_RIGHT_ARROW;
  else if (main.startsWith("F") && main.length() <= 3) {
    int fn = main.substring(1).toInt();
    if (fn >=1 && fn <= 12) code = KEY_F1 + (fn - 1);
  }

  // Press modifiers
  if (pressCtrl) Keyboard.press(KEY_LEFT_CTRL);
  if (pressShift) Keyboard.press(KEY_LEFT_SHIFT);
  if (pressAlt) Keyboard.press(KEY_LEFT_ALT);
  if (pressWin) Keyboard.press(KEY_LEFT_GUI);

  if (code) Keyboard.press(code);
  Keyboard.releaseAll();
  logMsg(String("Pressionado: ") + keyStr);
}

// Convert old CMD: style commands to new :cmd style
String normalizeCommand(const String &cmd) {
    String normalized = cmd;
    normalized.toLowerCase();
    
    // Convert old CMD:XXX to :cmd xxx format
    if (normalized.startsWith("cmd:")) {
        normalized = ":cmd " + normalized.substring(4);
    }

  // Convert legacy press: and type: to :press and :type
  if (normalized.startsWith("press:")) {
    normalized = ":press " + normalized.substring(6);
  }
  if (normalized.startsWith("type:")) {
    normalized = ":type " + normalized.substring(5);
  }
    
    // Convert specific old commands to new format
    if (normalized == ":cmd logto:on") normalized = ":cmd logto on";
    if (normalized == ":cmd logto:off") normalized = ":cmd logto off";
    if (normalized.startsWith(":cmd rsyslog:")) {
        normalized = ":cmd rsyslog " + normalized.substring(13);
    }
    
    return normalized;
}

// Process and type a text string with escapes: \n => ENTER, \\ => backslash
void processAndType(const String &txt) {
  for (size_t i = 0; i < txt.length(); ) {
    char c = txt.charAt(i);
    if (c == '\\' && (i + 1) < txt.length()) {
      char next = txt.charAt(i+1);
      if (next == 'n') {
        pressAndRelease(KEY_RETURN);
        logMsg("Pressionado: ENTER");
        delay(config.keyDelayMs);
        i += 2;
        continue;
      } else if (next == '\\') {
        Keyboard.print('\\');
        delay(config.keyDelayMs);
        i += 2;
        continue;
      } else {
        Keyboard.print(next);
        delay(config.keyDelayMs);
        i += 2;
        continue;
      }
    }
    Keyboard.print(c);
    delay(config.keyDelayMs);
    i++;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize configuration
  loadConfig();
  config.bootTime = millis();
  
  // Set hostname if configured
  if (config.hostname.length() > 0) {
      WiFi.setHostname(config.hostname.c_str());
  }

  // Inicializa o USB HID Keyboard
  // Certifique-se de que seu ESP32-S3 está configurado para USB CDC e HID
  USB.begin();
  Keyboard.begin();
  usbAttached = true;

  Serial.print("Conectando ao WiFi ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  logMsg("WiFi conectado!");
  logMsg(String("Endereço IP: ") + WiFi.localIP().toString());

  // Inicia o servidor TCP
  tcpServer.begin();
  logMsg(String("Servidor TCP iniciado na porta ") + String(port));
  logMsg("Aguardando conexão do cliente...");
}

void processCommand(const String &command) {
    // Process a command from any source (TCP or MQTT)
  if (command.startsWith("press:")) {
    handlePressCommand(command.substring(6));
  } else if (command.startsWith(":cmd")) {
    // forward to main loop handler by writing into same processing path
    // For simplicity, reuse normalizeCommand and then process inline
    String c = normalizeCommand(command);
    // emulate a received TCP command by calling the main processing block
    // We'll just handle a few commands here; others are processed in loop path
    if (c == ":cmd reboot") {
      logMsg("Reiniciando ESP32...");
      delay(100);
      ESP.restart();
    } else if (c == ":cmd status") {
      // delegate to same status code as in loop
      // Build a fake client command
      // (simpler approach: call the status code directly)
      unsigned long uptime = millis() - config.bootTime;
      unsigned long uptimeSec = uptime / 1000;
      unsigned long uptimeMin = uptimeSec / 60;
      unsigned long uptimeHour = uptimeMin / 60;
      uptimeMin %= 60; uptimeSec %= 60;
      logMsg("--- STATUS ---");
      logMsg(String("Hostname: ") + config.hostname);
      logMsg(String("WiFi IP: ") + WiFi.localIP().toString());
      logMsg(String("Uptime: ") + uptimeHour + "h " + uptimeMin + "m " + uptimeSec + "s");
      logMsg(String("Free heap: ") + String(ESP.getFreeHeap()) + " bytes");
      logMsg(String("Reset reason: ") + String(esp_reset_reason()));
    }
  } else {
    // type text
    processAndType(command);
  }
}

void loop() {
  // (no MQTT in this build) 
  
  // Verifica se há um novo cliente tentando se conectar
  if (tcpServer.hasClient()) {
    // Se já houver um cliente conectado, desconecta-o para aceitar o novo
    if (client && client.connected()) {
  logMsg("Cliente existente desconectado.");
      client.stop();
    }
  client = tcpServer.accept();
  logMsg("Novo cliente conectado!");
  
  // Send welcome message and output buffer
  client.println("=== ESP32 Keyboard Controller ===\r");
  client.println(String("Device: ") + config.hostname);
  client.println("\r=== Últimas mensagens ===\r");
  outputBuffer.sendTo(client);
  client.println("\r=== Digite um comando ou 'help' para ajuda ===\r");
  }

  // Se houver um cliente conectado e dados disponíveis
  if (client && client.connected() && client.available()) {
    String command = client.readStringUntil('\n');
    command.trim(); // Remove espaços em branco e caracteres de nova linha
  logMsg(String("Comando recebido: ") + command);

    // ...existing code...

    // Processa o comando e emula a digitação
    command = normalizeCommand(command);
    
    if (command.startsWith(":press ")) {
      String keyStr = command.substring(7);
      handlePressCommand(keyStr);
    } else if (command == ":cmd ota") {
      if (!otaEnabled) {
        otaEnabled = true;
        ArduinoOTA.setHostname(config.hostname.c_str());
        ArduinoOTA.begin();
        logMsg("OTA habilitado! Você pode enviar o firmware usando:");
        logMsg(String("python espota.py -i ") + WiFi.localIP().toString() + " -p 3232 --file esp32_keyboard.ino.bin");
      } else {
        logMsg("OTA já está habilitado.");
      }
    } else if (command == ":cmd logto on") {
        config.logToRsyslog = true;
        saveConfig();
        logMsg("Log para rsyslog ativado.");
    } else if (command == ":cmd logto off") {
        config.logToRsyslog = false;
        saveConfig();
        logMsg("Log para rsyslog desativado.");
    } else if (command.startsWith(":cmd logto ")) {
        logMsg("Opção inválida para logto. Use ':cmd logto on' ou ':cmd logto off'.");
    } else if (command.startsWith(":cmd rsyslog ")) {
      String server = command.substring(12);
      server.trim();
      config.rsyslogServer = server;
      saveConfig();
      logMsg(String("Servidor rsyslog configurado para: ") + config.rsyslogServer);
    } else if (command == ":cmd status") {
      // Mostra status atual das configurações
      unsigned long uptime = millis() - config.bootTime;
      unsigned long uptimeSec = uptime / 1000;
      unsigned long uptimeMin = uptimeSec / 60;
      unsigned long uptimeHour = uptimeMin / 60;
      uptimeMin %= 60;
      uptimeSec %= 60;
      
      logMsg("--- STATUS ---");
      logMsg(String("Hostname: ") + config.hostname);
      logMsg(String("WiFi IP: ") + WiFi.localIP().toString());
      logMsg(String("WiFi RSSI: ") + WiFi.RSSI() + " dBm");
      logMsg(String("Uptime: ") + uptimeHour + "h " + uptimeMin + "m " + uptimeSec + "s");
      logMsg(String("USB HID: ") + (usbAttached ? "CONNECTED" : "DISCONNECTED"));
      logMsg("--- Rsyslog Config ---");
      logMsg(String("Log to rsyslog: ") + (config.logToRsyslog ? "ON" : "OFF"));
      logMsg(String("Rsyslog server: ") + config.rsyslogServer);
      if (config.logToRsyslog) {
        logMsg(String("Rsyslog state: ") + 
               (rsyslog.temporarilyDisabled ? "DISABLED (too many failures)" :
                rsyslog.failedAttempts > 0 ? String("RETRY (") + rsyslog.failedAttempts + "/" + config.rsyslogMaxRetries + ")" :
                "OK"));
      }
      // Diagnostics
      logMsg(String("Reset reason: ") + String(esp_reset_reason()));
      logMsg(String("Free heap: ") + String(ESP.getFreeHeap()) + " bytes");
      logMsg("---------------");
    } else if (command == ":cmd reset") {
      // Restaura configurações padrão e limpa prefs
      prefs.begin("kbdcfg", false);
      prefs.clear();
      prefs.end();
      config.logToRsyslog = false;
      config.rsyslogServer = "192.168.5.2";
      config.hostname = "esp32kbd";
      saveConfig();
      logMsg("Configurações reiniciadas para o padrão.");
      logMsg("Config: RESET to defaults");
    } else if (command == ":cmd usb detach" || command == ":cmd disconnect") {
      if (usbAttached) {
        // Stop HID and USB
        Keyboard.end();
        usbAttached = false;
        logMsg("USB HID desconectado (DETACH).\n");
      } else {
        logMsg("USB já está desconectado.");
      }
    } else if (command == ":cmd usb attach" || command == ":cmd reconnect") {
      if (!usbAttached) {
        // Attempt to (re)initialize USB if available, then start keyboard
        // Some ESP32 cores expose USB.begin(), others manage USB automatically.
        // Calling Keyboard.begin() is the essential step for HID.
        #if defined(USB)
        USB.begin();
        #endif
        Keyboard.begin();
        usbAttached = true;
        logMsg("USB HID reconectado (ATTACH).\n");
      } else {
        logMsg("USB já está conectado.");
      }
    } else if (command == ":cmd usb toggle" || command == ":cmd toggleusb") {
      if (usbAttached) {
        Keyboard.end();
        usbAttached = false;
        logMsg("USB HID toggled -> DISCONNECTED.");
      } else {
        #if defined(USB)
        USB.begin();
        #endif
        Keyboard.begin();
        usbAttached = true;
        logMsg("USB HID toggled -> CONNECTED.");
      }
    } else if (command.startsWith(":cmd keydelay ")) {
      String msStr = command.substring(14);
      msStr.trim();
      int ms = msStr.toInt();
      if (ms >= 0 && ms <= 1000) {
        config.keyDelayMs = ms;
        saveConfig();
        logMsg(String("Delay entre teclas ajustado para: ") + ms + " ms");
      } else {
        logMsg("Valor inválido para delay. Use entre 0 e 1000 ms.");
      }
    } else if (command.startsWith(":cmd hostname ")) {
      String newHostname = command.substring(13);
      newHostname.trim();
      if (newHostname.length() > 0 && newHostname.length() <= 32) {
        config.hostname = newHostname;
        WiFi.setHostname(config.hostname.c_str());
        saveConfig();
        logMsg(String("Hostname configurado para: ") + config.hostname);
        logMsg("Reinicie o dispositivo para aplicar completamente as alterações de hostname");
      } else {
        logMsg("Hostname inválido. Use entre 1 e 32 caracteres.");
      }
    } else if (command == ":cmd reboot") {
      logMsg("Reiniciando ESP32...");
      delay(100);  // Give some time for the message to be sent
      ESP.restart();
    } else if (command == ":cmd help") {
      // Show help with available commands
      logMsg("--- HELP: Comandos Disponíveis ---");
  logMsg(":press <key>         - Pressiona uma tecla (enter,tab,esc,backspace,delete,space,win,ctrl+,alt,shift)");
  logMsg(":type <text>          - Digita o texto (use \\n para ENTER, \\\\ para barra invertida)");
  logMsg(":cmd keydelay <ms>    - Ajusta o delay entre teclas (ms, padrão 20)");
      logMsg(":cmd logto on|off   - Habilita/desabilita log para rsyslog");
      logMsg(":cmd rsyslog <ip>   - Define servidor rsyslog");
      logMsg(":cmd hostname <name> - Define o hostname do dispositivo");
  // MQTT commands removed in this build
      logMsg(":cmd status         - Mostra status atual");
      logMsg(":cmd reset          - Restaura configurações padrão");
      logMsg(":cmd usb detach     - Desconecta o HID USB");
      logMsg(":cmd usb attach     - Reconecta o HID USB");
      logMsg(":cmd usb toggle     - Alterna estado USB");
      logMsg(":cmd reboot         - Reinicia o ESP32");
      logMsg(":cmd help           - Mostra esta ajuda");
      logMsg("----------------------------------");
    // Restaura configurações padrão e limpa prefs
    prefs.begin("kbdcfg", false);
    prefs.clear();
    prefs.end();
    config.logToRsyslog = false;
    config.rsyslogServer = "192.168.5.2";
    saveConfig();
  logMsg("Configurações reiniciadas para o padrão.");
  logMsg("Config: RESET to defaults");
    } else {
      // Qualquer outro texto (sem necessidade de :type) será digitado
      String textToType = command;
      // Se a string começar com TYPE:, também deve funcionar
      if (textToType.startsWith(":type ")) {
        textToType = textToType.substring(6);
      }
      processAndType(textToType);
  logMsg(String("Digitando: ") + textToType);
    }
  }

  // Se OTA estiver habilitado, rode o handler
  if (otaEnabled) {
    ArduinoOTA.handle();
  }
  // Pequeno atraso para evitar sobrecarga da CPU
  delay(10);
}


