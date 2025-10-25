//
// Arquivo: config.cpp
// Implementação das variáveis globais definidas em config.h
//
#include "config.h"

// Instância do buffer circular para armazenamento de mensagens
// Inicializado com valores padrão (vazio)
OutputBuffer outputBuffer;

// Instância das configurações do sistema
// Os valores iniciais são definidos na inicialização do sistema
Config config;

// Estado do sistema de logging remoto
// Inicializado com valores padrão (desabilitado)
RsyslogState rsyslog;

// Mutex para sincronização de acesso ao teclado USB HID
// Inicializado como NULL, será criado durante a inicialização do sistema
SemaphoreHandle_t keyboardMutex = NULL;
