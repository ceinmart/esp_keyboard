// Arquivo: config.h
// Definições das estruturas principais de configuração e estado do sistema
// Inclui configurações persistentes e estados de execução
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
// Semáforo FreeRTOS para proteger acesso ao USB HID
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Tamanho do buffer circular para histórico de mensagens TCP
static constexpr int TCP_BUFFER_SIZE = 50;

// Buffer circular para armazenar mensagens recentes
// Usado para mostrar histórico quando um cliente TCP se conecta
struct OutputBuffer {
  String lines[TCP_BUFFER_SIZE];  // Array de mensagens
  int start;                      // Posição inicial do buffer circular
  int count;                      // Quantidade de mensagens armazenadas
};

// Configurações do sistema que são persistidas na memória flash
// Estas configurações podem ser alteradas via comandos e são salvas
struct Config {
  bool logToRsyslog;              // Se verdadeiro, envia logs para servidor remoto
  String rsyslogServer;           // Endereço IP ou hostname do servidor de log
  String hostname;                // Nome do dispositivo na rede
  unsigned long bootTime;         // Timestamp do início do sistema
  uint8_t rsyslogMaxRetries;     // Número máximo de tentativas antes de desabilitar rsyslog
  uint16_t keyDelayMs;           // Atraso entre pressionamentos de tecla em milissegundos
};

// Estado do sistema de log remoto (rsyslog)
// Controla tentativas de conexão e estado temporário
struct RsyslogState {
  bool enabled;                  // Se o sistema de log está ativo
  uint8_t failedAttempts;       // Contador de falhas consecutivas
  unsigned long lastAttempt;     // Timestamp da última tentativa de envio
  bool temporarilyDisabled;      // Desabilitado temporariamente após muitas falhas
};

// Variáveis globais do sistema
// Definidas em config.cpp, declaradas aqui para acesso global
extern OutputBuffer outputBuffer;  // Buffer circular de mensagens
extern Config config;              // Configurações do sistema
extern RsyslogState rsyslog;      // Estado do sistema de log
// Mutex para proteger operações do teclado USB entre tarefas
extern SemaphoreHandle_t keyboardMutex;
