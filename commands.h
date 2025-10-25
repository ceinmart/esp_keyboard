//
// Arquivo: commands.h
// Sistema de processamento de comandos do teclado remoto
// Gerencia todos os comandos disponíveis via interface TCP
//
#pragma once
#include <Arduino.h>

// Inicializa o sistema de comandos
// Configura a tabela de comandos disponíveis e seus handlers
void initCommands();

// Processa um comando recebido via TCP
// Interpreta e executa comandos como :press, :type, :cmd, etc
// Os comandos podem alterar configurações ou controlar o teclado
void processCommand(const String &command);