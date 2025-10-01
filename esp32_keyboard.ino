
#include <WiFi.h>
#include <WiFiClient.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

USBHIDKeyboard Keyboard;

// Substitua pelas suas credenciais de WiFi
const char* ssid = "dmartins";
const char* password = "192@dmartins";

// Porta para o servidor TCP
const uint16_t port = 1234;
WiFiServer tcpServer(port);
WiFiClient client;

void setup() {
  Serial.begin(115200);

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
  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // Inicia o servidor TCP
  tcpServer.begin();
  Serial.print("Servidor TCP iniciado na porta ");
  Serial.println(port);
  Serial.println("Aguardando conexão do cliente...");
}

void loop() {
  // Verifica se há um novo cliente tentando se conectar
  if (tcpServer.hasClient()) {
    // Se já houver um cliente conectado, desconecta-o para aceitar o novo
    if (client && client.connected()) {
      Serial.println("Cliente existente desconectado.");
      client.stop();
    }
    client = tcpServer.available();
    Serial.println("Novo cliente conectado!");
  }

  // Se houver um cliente conectado e dados disponíveis
  if (client && client.connected() && client.available()) {
    String command = client.readStringUntil('\n');
    command.trim(); // Remove espaços em branco e caracteres de nova linha
    Serial.print("Comando recebido: ");
    Serial.println(command);

    // Processa o comando e emula a digitação
    if (command.startsWith("TYPE:")) {
      String textToType = command.substring(5);
      Keyboard.print(textToType);
      Serial.print("Digitando: ");
      Serial.println(textToType);
    } else if (command.startsWith("PRESS:")) {
      String keyStr = command.substring(6);
      if (keyStr == "ENTER") {
        Keyboard.press(KEY_RETURN);
        Keyboard.releaseAll();
        Serial.println("Pressionado: ENTER");
      } else if (keyStr == "TAB") {
        Keyboard.press(KEY_TAB);
        Keyboard.releaseAll();
        Serial.println("Pressionado: TAB");
      } else if (keyStr == "ESC") {
        Keyboard.press(KEY_ESC);
        Keyboard.releaseAll();
        Serial.println("Pressionado: ESC");
      } else if (keyStr == "BACKSPACE") {
        Keyboard.press(KEY_BACKSPACE);
        Keyboard.releaseAll();
        Serial.println("Pressionado: BACKSPACE");
      } else if (keyStr == "DELETE") {
        Keyboard.press(KEY_DELETE);
        Keyboard.releaseAll();
        Serial.println("Pressionado: DELETE");
      } else if (keyStr == "SPACE") {
        Keyboard.press(' ');
        Keyboard.releaseAll();
        Serial.println("Pressionado: SPACE");
      } else if (keyStr.startsWith("CTRL+")) {
        String modKey = keyStr.substring(5);
        Keyboard.press(KEY_LEFT_CTRL);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        } else if (modKey == "ALT") {
          Keyboard.press(KEY_LEFT_ALT);
        } // Adicione mais combinações se necessário
        Keyboard.releaseAll();
        Serial.print("Pressionado: CTRL+");
        Serial.println(modKey);
      } else if (keyStr.startsWith("ALT+")) {
        String modKey = keyStr.substring(4);
        Keyboard.press(KEY_LEFT_ALT);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        }
        Keyboard.releaseAll();
        Serial.print("Pressionado: ALT+");
        Serial.println(modKey);
      } else if (keyStr.startsWith("SHIFT+")) {
        String modKey = keyStr.substring(6);
        Keyboard.press(KEY_LEFT_SHIFT);
        if (modKey.length() == 1) {
          Keyboard.press(modKey.charAt(0));
        }
        Keyboard.releaseAll();
        Serial.print("Pressionado: SHIFT+");
        Serial.println(modKey);
      } else if (keyStr.length() == 1) {
        // Para caracteres únicos (letras, números, símbolos)
        Keyboard.press(keyStr.charAt(0));
        Keyboard.releaseAll();
        Serial.print("Pressionado: ");
        Serial.println(keyStr.charAt(0));
      } else {
        Serial.print("Comando PRESS desconhecido: ");
        Serial.println(keyStr);
      }
    } else {
      Serial.print("Comando desconhecido: ");
      Serial.println(command);
    }
  }

  // Pequeno atraso para evitar sobrecarga da CPU
  delay(10);
}


