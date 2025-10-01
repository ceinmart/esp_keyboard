
#include <WiFi.h>
#include <WiFiClient.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
Preferences prefs;
bool logToRsyslog = false;
String rsyslogServer = "192.168.1.100";
const uint16_t rsyslogPort = 514;
WiFiUDP rsyslogUdp;

void sendToRsyslog(String msg) {
  if (!logToRsyslog) return;
  rsyslogUdp.beginPacket(rsyslogServer.c_str(), rsyslogPort);
  rsyslogUdp.write(msg.c_str());
  rsyslogUdp.endPacket();
}

// Unified logging: prints to Serial and, if enabled, to rsyslog
void logMsg(const String &msg) {
  Serial.println(msg);
  if (logToRsyslog) sendToRsyslog(msg);
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

  prefs.begin("kbdcfg", false);
  logToRsyslog = prefs.getBool("logToRsyslog", false);
  rsyslogServer = prefs.getString("rsyslogServer", "192.168.1.100");

  // Inicializa o USB HID Keyboard
  // Certifique-se de que seu ESP32-S3 está configurado para USB CDC e HID
  USB.begin();
  Keyboard.begin();

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

void loop() {
  // ...existing code...
  // Verifica se há um novo cliente tentando se conectar
  if (tcpServer.hasClient()) {
    // Se já houver um cliente conectado, desconecta-o para aceitar o novo
    if (client && client.connected()) {
  logMsg("Cliente existente desconectado.");
      client.stop();
    }
    client = tcpServer.available();
  logMsg("Novo cliente conectado!");
  }

  // Se houver um cliente conectado e dados disponíveis
  if (client && client.connected() && client.available()) {
    String command = client.readStringUntil('\n');
    command.trim(); // Remove espaços em branco e caracteres de nova linha
  logMsg(String("Comando recebido: ") + command);

    // ...existing code...

    // Processa o comando e emula a digitação
    if (command.startsWith("PRESS:")) {
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
    } else if (command.startsWith("CMD:LOGTO:")) {
      String opt = command.substring(10);
      opt.trim();
      if (opt == "on") {
        logToRsyslog = true;
        prefs.putBool("logToRsyslog", true);
  logMsg("Log para rsyslog ativado.");
      } else if (opt == "off") {
        logToRsyslog = false;
        prefs.putBool("logToRsyslog", false);
  logMsg("Log para rsyslog desativado.");
        // Not sending to rsyslog because it's disabled
      } else {
        logMsg("Opção inválida para LOGTO. Use on ou off.");
      }
    } else if (command.startsWith("CMD:RSYSLOG:")) {
      String server = command.substring(12);
      server.trim();
      rsyslogServer = server;
      prefs.putString("rsyslogServer", rsyslogServer);
  logMsg(String("Servidor rsyslog configurado para: ") + rsyslogServer);
    } else if (command == "CMD:STATUS") {
      // Mostra status atual das configurações
  logMsg("--- STATUS ---");
  logMsg(String("WiFi IP: ") + WiFi.localIP().toString());
  logMsg(String("Log to rsyslog: ") + (logToRsyslog ? "ON" : "OFF"));
  logMsg(String("Rsyslog server: ") + rsyslogServer);
  logMsg("---------------");
    } else if (command == "CMD:RESET") {
      // Restaura configurações padrão e limpa prefs
      prefs.clear();
      logToRsyslog = false;
      rsyslogServer = "192.168.1.100";
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


