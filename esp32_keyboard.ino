//TODO: adicionar no codigo tags de versão e data/hora de compilação e revisão do git, exibir estas informações no comando de status.
//TODO: adicionar a configuração do wifi no prefs , mantendo os valores atuais como default.
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
    
    // MQTT Configuration
    bool mqttEnabled;
    String mqttServer;
    uint16_t mqttPort;
    String mqttUser;
    String mqttPassword;
    String mqttPrefix;  // Topic prefix for Home Assistant
} config;

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
// MQTT callback when messages are received
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    msg.trim();
    
    if (String(topic) == mqttCommandTopic) {
        // Process command as if it came from TCP
        logMsg(String("MQTT command: ") + msg);
        processCommand(msg);
    }
}

// Publish MQTT discovery config for Home Assistant
void publishMqttDiscovery() {
    if (!config.mqttEnabled || !mqtt.connected()) return;
    
    String configPayload = "{\
        \"name\": \"" + config.hostname + "\",\
        \"unique_id\": \"esp32kbd_" + String((uint32_t)ESP.getEfuseMac(), HEX) + "\",\
        \"command_topic\": \"" + String(mqttCommandTopic) + "\",\
        \"state_topic\": \"" + String(mqttStatusTopic) + "\",\
        \"availability_topic\": \"" + String(mqttAvailability) + "\",\
        \"payload_available\": \"online\",\
        \"payload_not_available\": \"offline\",\
        \"device\": {\
            \"identifiers\": [\"esp32kbd_" + String((uint32_t)ESP.getEfuseMac(), HEX) + "\"],\
            \"name\": \"ESP32 Keyboard\",\
            \"model\": \"ESP32-S3 USB HID\",\
            \"manufacturer\": \"DIY\"\
        }\
    }";
    
    mqtt.publish(mqttConfigTopic, configPayload.c_str(), true);
    mqtt.publish(mqttAvailability, "online", true);
}

// Connect/reconnect to MQTT broker
bool connectMqtt() {
    if (!config.mqttEnabled) return false;
    
    if (mqtt.connected()) return true;
    
    mqtt.setServer(config.mqttServer.c_str(), config.mqttPort);
    mqtt.setCallback(mqttCallback);
    
    String clientId = "ESP32KBD-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);
    
    bool success = false;
    if (config.mqttUser.length() > 0) {
        success = mqtt.connect(clientId.c_str(), 
                             config.mqttUser.c_str(),
                             config.mqttPassword.c_str(),
                             mqttAvailability,
                             0,
                             true,
                             "offline");
    } else {
        success = mqtt.connect(clientId.c_str(),
                             mqttAvailability,
                             0,
                             true,
                             "offline");
    }
    
    if (success) {
        mqtt.subscribe(mqttCommandTopic);
        publishMqttDiscovery();
        logMsg("MQTT conectado");
    }
    
    return success;
}

void loadConfig() {
    if (!prefs.begin("kbdcfg", false)) {
        logMsg("Failed to initialize Preferences");
        return;
    }
    config.logToRsyslog = prefs.getBool("logToRsyslog", false);
    config.rsyslogServer = prefs.getString("rsyslogServer", "192.168.5.2");
    config.hostname = prefs.getString("hostname", "esp32kbd");
    config.rsyslogMaxRetries = prefs.getUChar("rsyslogRetries", 3);
    
    // Load MQTT config
    config.mqttEnabled = prefs.getBool("mqttEnabled", false);
    config.mqttServer = prefs.getString("mqttServer", "");
    config.mqttPort = prefs.getUShort("mqttPort", 1883);
    config.mqttUser = prefs.getString("mqttUser", "");
    config.mqttPassword = prefs.getString("mqttPass", "");
    config.mqttPrefix = prefs.getString("mqttPrefix", "homeassistant");
    
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
    
    // Save MQTT config
    prefs.putBool("mqttEnabled", config.mqttEnabled);
    prefs.putString("mqttServer", config.mqttServer);
    prefs.putUShort("mqttPort", config.mqttPort);
    prefs.putString("mqttUser", config.mqttUser);
    prefs.putString("mqttPass", config.mqttPassword);
    prefs.putString("mqttPrefix", config.mqttPrefix);
    
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

// Convert old CMD: style commands to new :cmd style
String normalizeCommand(const String &cmd) {
    String normalized = cmd;
    normalized.toLowerCase();
    
    // Convert old CMD:XXX to :cmd xxx format
    if (normalized.startsWith("cmd:")) {
        normalized = ":cmd " + normalized.substring(4);
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
        i += 2;
        continue;
      } else if (next == '\\') {
        Keyboard.print('\\');
        i += 2;
        continue;
      } else {
        Keyboard.print(next);
        i += 2;
        continue;
      }
    }
    Keyboard.print(c);
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
        // ... existing press handling ...
    } else if (command.startsWith(":cmd")) {
        // ... existing command handling ...
    } else {
        // ... existing default type handling ...
    }
}

void loop() {
    // Handle MQTT connection/messages
    if (config.mqttEnabled) {
        if (!mqtt.connected()) {
            connectMqtt();
        }
        mqtt.loop();
    }
  // Handle MQTT connection/messages
  if (config.mqttEnabled) {
    if (!mqtt.connected()) {
      connectMqtt();
    }
    mqtt.loop();
  }
  
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
    
    if (command.startsWith("press:")) {
      String keyStr = command.substring(6);
      if (keyStr == "ENTER") {
        Keyboard.press(KEY_RETURN);
        Keyboard.releaseAll();
  logMsg("Pressionado: ENTER");
      } else if (keyStr == "TAB") {
        Keyboard.press(KEY_TAB);
        Keyboard.releaseAll();
  logMsg("Pressionado: TAB");
      } else if (keyStr == "ESC") {
        Keyboard.press(KEY_ESC);
        Keyboard.releaseAll();
  logMsg("Pressionado: ESC");
      } else if (keyStr == "BACKSPACE") {
        Keyboard.press(KEY_BACKSPACE);
        Keyboard.releaseAll();
  logMsg("Pressionado: BACKSPACE");
      } else if (keyStr == "DELETE") {
        Keyboard.press(KEY_DELETE);
        Keyboard.releaseAll();
  logMsg("Pressionado: DELETE");
      } else if (keyStr == "SPACE") {
        Keyboard.press(' ');
        Keyboard.releaseAll();
  logMsg("Pressionado: SPACE");
      } else if (keyStr == "WIN") {
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.releaseAll();
  logMsg("Pressionado: WIN");
      } else if (keyStr.startsWith("WIN+")) {
        String modKey = keyStr.substring(4);
        Keyboard.press(KEY_LEFT_GUI);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        }
        Keyboard.releaseAll();
  logMsg(String("Pressionado: WIN+") + modKey);
      } else if (keyStr.startsWith("CTRL+")) {
        String modKey = keyStr.substring(5);
        Keyboard.press(KEY_LEFT_CTRL);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        } else if (modKey == "ALT") {
          Keyboard.press(KEY_LEFT_ALT);
        } // Adicione mais combinações se necessário
        Keyboard.releaseAll();
  logMsg(String("Pressionado: CTRL+") + modKey);
      } else if (keyStr.startsWith("ALT+")) {
        String modKey = keyStr.substring(4);
        Keyboard.press(KEY_LEFT_ALT);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        }
        Keyboard.releaseAll();
  logMsg(String("Pressionado: ALT+") + modKey);
      } else if (keyStr.startsWith("SHIFT+")) {
        String modKey = keyStr.substring(6);
        Keyboard.press(KEY_LEFT_SHIFT);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        }
        Keyboard.releaseAll();
  logMsg(String("Pressionado: SHIFT+") + modKey);
      } else if (keyStr.length() == 1) {
        // Para caracteres únicos (letras, números, símbolos)
        Keyboard.press(keyStr.charAt(0));
        Keyboard.releaseAll();
  logMsg(String("Pressionado: ") + keyStr.charAt(0));
      } else {
  logMsg(String("Comando PRESS desconhecido: ") + keyStr);
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
        logMsg("Opção inválida para logto. Use on ou off.");
      opt.trim();
      if (opt == "on") {
        config.logToRsyslog = true;
        saveConfig();
        logMsg("Log para rsyslog ativado.");
      } else if (opt == "off") {
        config.logToRsyslog = false;
        saveConfig();
        logMsg("Log para rsyslog desativado.");
        // Not sending to rsyslog because it's disabled
      } else {
        logMsg("Opção inválida para LOGTO. Use on ou off.");
      }
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
      logMsg("---------------");
    } else if (command == ":cmd reset") {
      // USB detach/attach commands
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
    } else if (command == ":cmd mqtt status") {
        logMsg("--- MQTT Status ---");
        logMsg(String("Enabled: ") + (config.mqttEnabled ? "yes" : "no"));
        if (config.mqttEnabled) {
            logMsg(String("Server: ") + config.mqttServer + ":" + config.mqttPort);
            logMsg(String("Connected: ") + (mqtt.connected() ? "yes" : "no"));
            logMsg(String("Username: ") + (config.mqttUser.length() > 0 ? config.mqttUser : "<none>"));
        }
    } else if (command.startsWith(":cmd mqtt server ")) {
        String server = command.substring(16);
        int portPos = server.indexOf(":");
        if (portPos > 0) {
            config.mqttPort = server.substring(portPos + 1).toInt();
            config.mqttServer = server.substring(0, portPos);
        } else {
            config.mqttServer = server;
            config.mqttPort = 1883;
        }
        saveConfig();
        logMsg(String("MQTT server configurado: ") + config.mqttServer + ":" + config.mqttPort);
    } else if (command.startsWith(":cmd mqtt auth ")) {
        String auth = command.substring(14);
        int sepPos = auth.indexOf(" ");
        if (sepPos > 0) {
            config.mqttUser = auth.substring(0, sepPos);
            config.mqttPassword = auth.substring(sepPos + 1);
            saveConfig();
            logMsg("MQTT credentials configuradas");
        } else {
            logMsg("Formato: :cmd mqtt auth <username> <password>");
        }
    } else if (command == ":cmd mqtt enable") {
        config.mqttEnabled = true;
        saveConfig();
        connectMqtt();
        logMsg("MQTT enabled");
    } else if (command == ":cmd mqtt disable") {
        config.mqttEnabled = false;
        if (mqtt.connected()) mqtt.disconnect();
        saveConfig();
        logMsg("MQTT disabled");
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
      logMsg("press:<key>         - Pressiona uma tecla (enter,tab,esc,backspace,delete,space,win,ctrl+,alt,shift)");
      logMsg("type:<text>         - Digita o texto (use \\n for ENTER escape, \\\\ for backslash)");
      logMsg(":cmd logto on|off   - Habilita/desabilita log para rsyslog");
      logMsg(":cmd rsyslog <ip>   - Define servidor rsyslog");
      logMsg(":cmd hostname <name> - Define o hostname do dispositivo");
      logMsg(":cmd mqtt status    - Mostra status do MQTT");
      logMsg(":cmd mqtt enable    - Habilita MQTT");
      logMsg(":cmd mqtt disable   - Desabilita MQTT");
      logMsg(":cmd mqtt server    - Define servidor MQTT (host:port)");
      logMsg(":cmd mqtt auth      - Define credenciais MQTT (user pass)");
      logMsg(":cmd status         - Mostra status atual");
      logMsg(":cmd reset          - Restaura configurações padrão");
      logMsg(":cmd usb detach     - Desconecta o HID USB");
      logMsg(":cmd usb attach     - Reconecta o HID USB");
      logMsg(":cmd usb toggle     - Alterna estado USB");
      logMsg(":cmd reboot         - Reinicia o ESP32");
      logMsg(":cmd help           - Mostra esta ajuda");
      logMsg("----------------------------------");
      // Restaura configurações padrão e limpa prefs
      prefs.clear();
      logToRsyslog = false;
      rsyslogServer = "192.168.5.2";
      prefs.putBool("logToRsyslog", logToRsyslog);
      prefs.putString("rsyslogServer", rsyslogServer);
  logMsg("Configurações reiniciadas para o padrão.");
  logMsg("Config: RESET to defaults");
    } else {
      // Qualquer outro texto (sem necessidade de TYPE:) será digitado
      String textToType = command;
      // Se a string começar com TYPE:, também deve funcionar
      if (textToType.startsWith("TYPE:")) {
        textToType = textToType.substring(5);
      }
      processAndType(textToType);
  logMsg(String("Digitando: ") + textToType);
    }
  }

  // Pequeno atraso para evitar sobrecarga da CPU
  delay(10);
}


