//
// Arquivo: wifi_mgr.h
// Módulo de gerenciamento de conexão WiFi
// Responsável pela conexão inicial e manutenção da conexão WiFi
//
#pragma once
#include <Arduino.h>
#include <WiFi.h>

// Inicializa a conexão WiFi com as credenciais fornecidas
// Tenta conectar por até 2 minutos antes de desistir
void initWiFi(const char* ssid,      // Nome da rede WiFi
              const char* password);  // Senha da rede WiFi

// Gerencia a conexão WiFi em execução
// Deve ser chamada periodicamente no loop principal
// Monitora a conexão e tenta reconectar se necessário
void handleWiFi();