//
// Arquivo: mqtt_mgr.cpp
// Implementação do gerenciador de conexão MQTT
//
#include "mqtt_mgr.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h"
#include "logging.h"
#include "commands.h"

// Cliente WiFi para a conexão MQTT
WiFiClient mqttWifiClient;
// Cliente MQTT
PubSubClient mqttClient(mqttWifiClient);

static unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_DELAY = 5000; // 5 segundos

// Protótipos de funções locais
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnectMqtt();

// Função de callback para mensagens recebidas
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String command;
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  command.trim();

  logMsg(String(F("Comando recebido via MQTT no tópico [")) + topic + F("]: ") + command);
  
  // Processa o comando como se viesse do TCP
  processCommand(command);
}

// Inicializa o cliente MQTT
void initMqtt() {
  if (config.mqttEnabled && config.mqttServer.length() > 0) {
    mqttClient.setServer(config.mqttServer.c_str(), config.mqttPort);
    mqttClient.setCallback(mqttCallback);
    logMsg(F("Cliente MQTT inicializado."));
  }
}

// Tenta reconectar ao broker MQTT
void reconnectMqtt() {
  logMsg(F("Tentando conectar ao broker MQTT..."));
  String clientId = config.hostname + F("-") + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str(), config.mqttUser.c_str(), config.mqttPassword.c_str())) {
    logMsg(F("Conectado ao broker MQTT!"));
    
    // Publica mensagem de status online
    String statusTopic = config.mqttBaseTopic + F("/status");
    publishMqtt(statusTopic, F("online"), true);

    // Subscreve ao tópico de comandos
    String commandTopic = config.mqttBaseTopic + F("/command");
    if (mqttClient.subscribe(commandTopic.c_str())) {
      logMsg(String(F("Subscrito no tópico: ")) + commandTopic);
    } else {
      logMsg(String(F("Falha ao subscrever no tópico: ")) + commandTopic);
    }
  } else {
    logMsg(String(F("Falha na conexão MQTT, rc=")) + mqttClient.state() + F(". Nova tentativa em 5 segundos."));
  }
}

// Gerencia a conexão e o loop do cliente MQTT
void handleMqtt() {
  if (!config.mqttEnabled || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > MQTT_RECONNECT_DELAY) {
      lastMqttReconnectAttempt = now;
      reconnectMqtt();
    }
  } else {
    mqttClient.loop();
  }
}

// Publica uma mensagem MQTT
void publishMqtt(const String &subTopic, const String &payload, bool retained) {
  if (!mqttClient.connected()) {
    return;
  }
  String topic = config.mqttBaseTopic + F("/") + subTopic;
  mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}
