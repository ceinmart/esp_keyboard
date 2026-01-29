//
// Arquivo: mqtt_mgr.h
// Módulo de gerenciamento de conexão MQTT
//
#pragma once
#include <Arduino.h>

// Inicializa o cliente MQTT com as configurações carregadas
void initMqtt();

// Gerencia a conexão MQTT, deve ser chamada no loop principal
// Trata reconexão e processa mensagens recebidas
void handleMqtt();

// Publica uma mensagem em um sub-tópico do tópico base
void publishMqtt(const String &subTopic, const String &payload, bool retained);
