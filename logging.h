//
// Arquivo: logging.h
// Sistema de logging com suporte a múltiplos destinos:
// - Serial (USB CDC)
// - Cliente TCP
// - Servidor Rsyslog remoto via UDP
//
#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include "config.h"

// Forward declarations de estruturas globais
struct Config;
struct RsyslogState;

// Interface principal de logging
void initLogging();                     // Inicializa o sistema de logging
void recheckSerial();                   // Verifica e reconecta Serial se necessário
void logMsg(const String &msg);         // Envia mensagem para todos destinos ativos

// Funções de gerenciamento do Rsyslog
void handleRsyslogError(const char *error);  // Trata erros de conexão com rsyslog
void sendToRsyslog(String msg);              // Envia mensagem específica para rsyslog
void enableRsyslog();                        // Reativa rsyslog após desabilitação
void setRsyslogDebug(bool enabled);          // Ativa/desativa debug do rsyslog
bool isRsyslogDebug();                       // Retorna estado do debug do rsyslog

// Variáveis globais externas (definidas em esp32_keyboard.ino)
extern bool serialEnabled;                      // Estado da porta Serial
extern WiFiClient client;                       // Cliente TCP conectado
extern RsyslogState rsyslog;                   // Estado do sistema rsyslog
extern WiFiUDP rsyslogUdp;                     // Socket UDP para rsyslog
extern const uint16_t rsyslogPort;             // Porta padrão do rsyslog (514)
extern const unsigned long RSYSLOG_RETRY_DELAY; // Atraso entre tentativas