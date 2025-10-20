#pragma once
#include <Arduino.h>
#include <WiFi.h>

void initWiFi(const char* ssid, const char* password);
void handleWiFi(); // chamada periódica no loop