#include "commands.h"
#include "logging.h"
#include "usbkbd.h"
#include "wifi_mgr.h"

#include <map>
#include <functional>
#include <USBHIDKeyboard.h>
#include <Preferences.h>
#include "esp_system.h"
#include "version.h"
#include <ArduinoOTA.h>

extern USBHIDKeyboard Keyboard;
extern bool usbAttached;
extern bool otaEnabled;
extern RsyslogState rsyslog;
extern Preferences prefs;

// Normaliza comandos antigos
static String normalizeCommand(const String &cmd) {
  String normalized = cmd;
  normalized.toLowerCase();

  if (normalized.startsWith(F("cmd:"))) {
    normalized = String(F(":cmd ")) + normalized.substring(4);
  }
  if (normalized.startsWith(F("press:"))) {
    normalized = String(F(":press ")) + normalized.substring(6);
  }
  if (normalized.startsWith(F("type:"))) {
    normalized = String(F(":type ")) + normalized.substring(5);
  }
  if (normalized == F(":cmd logto:on")) normalized = F(":cmd logto on");
  if (normalized == F(":cmd logto:off")) normalized = F(":cmd logto off");
  if (normalized.startsWith(F(":cmd rsyslog:"))) {
    normalized = String(F(":cmd rsyslog ")) + normalized.substring(13);
  }
  return normalized;
}

// Tabela
static std::map<String, std::function<void(const String&)>> commandTable;

static void cmdStatus(const String&) {
  unsigned long uptime = millis() - config.bootTime;
  unsigned long uptimeSec = uptime / 1000;
  unsigned long uptimeMin = uptimeSec / 60;
  unsigned long uptimeHour = uptimeMin / 60;
  uptimeMin %= 60;
  uptimeSec %= 60;

  logMsg(F("--- STATUS ---"));
  logMsg(String(F("Hostname: ")) + config.hostname);
  logMsg(String(F("WiFi IP: ")) + WiFi.localIP().toString());
  logMsg(String(F("WiFi RSSI: ")) + WiFi.RSSI() + F(" dBm"));
  logMsg(String(F("Uptime: ")) + uptimeHour + "h " + uptimeMin + "m " + uptimeSec + "s");
  // Report USB connection status per-port when possible. Many boards expose
  // both a CDC (COM) port and an OTG port.
  // Check actual Serial connection status instead of just the flag
  bool comConnected = Serial ? true : false;
  bool otgConnected = usbAttached; // OTG port status is tracked independently

  logMsg(String(F("USB HID (overall): ")) + (usbAttached ? F("CONNECTED") : F("DISCONNECTED")));
  logMsg(String(F("USB COM (CDC): ")) + (comConnected ? F("CONNECTED") : F("DISCONNECTED")));
  logMsg(String(F("USB OTG (HID): ")) + (otgConnected ? F("CONNECTED") : F("DISCONNECTED")));
  logMsg(String(F("Git: commit ")) + GIT_COMMIT + F(" | branch ") + GIT_BRANCH);
  logMsg(String(F("Build: ")) + GIT_DATE);
  logMsg(String(F("Path: ")) + GIT_PATH);
  logMsg(F("--- Rsyslog Config ---"));
  logMsg(String(F("Log to rsyslog: ")) + (config.logToRsyslog ? F("ON") : F("OFF")));
  logMsg(String(F("Rsyslog server: ")) + config.rsyslogServer);
  if (config.logToRsyslog) {
    String statusStr =
    rsyslog.temporarilyDisabled ? String(F("DISABLED (too many failures)")) :
    (rsyslog.failedAttempts > 0
      ? (String(F("RETRY (")) + rsyslog.failedAttempts + "/" + config.rsyslogMaxRetries + ")")
      : String(F("OK")));
  logMsg(String(F("Rsyslog debug: ")) + (isRsyslogDebug() ? F("ON") : F("OFF")));
    logMsg(String(F("Rsyslog state: ")) + statusStr);
  }
  logMsg(String(F("SDK: ")) + ESP.getSdkVersion());
  logMsg(String(F("Free heap: ")) + String(ESP.getFreeHeap()) + F(" bytes"));
  if (psramFound()) {
    logMsg(String(F("Free PSRAM: ")) + String(ESP.getFreePsram()) + F(" bytes"));
  }
  logMsg(String(F("Reset reason: ")) + String(esp_reset_reason()));
  logMsg(F("---------------"));
}

static void cmdReboot(const String&) {
  logMsg(F("Reiniciando ESP32..."));
  vTaskDelay(1);
  ESP.restart();
}

static void cmdHelp(const String&) {
  logMsg(F("--- HELP: Comandos Disponíveis ---"));
  logMsg(F(":press <key>         - Pressiona uma tecla (enter,tab,esc,backspace,delete,space,win,ctrl,alt,shift)"));
  logMsg(F(":type <text>         - Digita o texto (use \\n para ENTER, \\\\ para barra invertida)"));
  logMsg(F(":cmd keydelay <ms>   - Ajusta o delay entre teclas (0..1000 ms)"));
  logMsg(F(":cmd logto on|off    - Habilita/desabilita log para rsyslog"));
  logMsg(F(":cmd rsyslog <ip>    - Define servidor rsyslog"));
  logMsg(F(":cmd rsyslog enable  - Re-enable rsyslog (clears failures and sends test message)"));
  logMsg(F(":cmd rsyslog debug on|off - Toggle rsyslog debug sequence/log sends"));
  logMsg(F(":cmd ota [on|off]    - Habilita/desabilita atualização OTA (toggle se sem parâmetro)"));
  logMsg(F(":cmd hostname <name> - Define o hostname do dispositivo"));
  logMsg(F(":cmd status          - Mostra status atual"));
  logMsg(F(":cmd reset           - Restaura configurações padrão"));
  logMsg(F(":cmd exit|sair|quit  - Desconecta o cliente TCP conectado"));
  logMsg(F(":cmd usb detach      - Desconecta o HID USB"));
  logMsg(F(":cmd usb attach      - Reconecta o HID USB"));
  logMsg(F(":cmd usb toggle      - Alterna estado USB"));
  logMsg(F(":cmd reboot          - Reinicia o ESP32"));
  logMsg(F(":cmd help            - Mostra esta ajuda"));
  logMsg(F("----------------------------------"));
}

static void cmdOta(const String& params) {
  String p = params; p.trim();
  if (p == F("on") || p == F("enable")) {
    if (otaEnabled) {
      logMsg(F("OTA já está habilitado."));
      return;
    }
    // Configure ArduinoOTA callbacks
    ArduinoOTA.onStart([]() { logMsg(F("OTA start")); });
    ArduinoOTA.onEnd([]() { logMsg(F("OTA end")); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      logMsg(String(F("OTA progress: ")) + (progress * 100 / total) + "%");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      logMsg(String(F("OTA error: ")) + error);
    });
    ArduinoOTA.begin();
    otaEnabled = true;
    logMsg(F("OTA habilitado."));
  } else if (p == F("off") || p == F("disable")) {
    if (!otaEnabled) {
      logMsg(F("OTA já está desabilitado."));
      return;
    }
#if defined(ArduinoOTA_h)
    ArduinoOTA.end();
#endif
    otaEnabled = false;
    logMsg(F("OTA desabilitado."));
  } else {
    // Toggle
    if (otaEnabled) {
#if defined(ArduinoOTA_h)
      ArduinoOTA.end();
#endif
      otaEnabled = false;
      logMsg(F("OTA desabilitado."));
    } else {
      ArduinoOTA.onStart([]() { logMsg(F("OTA start")); });
      ArduinoOTA.onEnd([]() { logMsg(F("OTA end")); });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        logMsg(String(F("OTA progress: ")) + (progress * 100 / total) + "%");
      });
      ArduinoOTA.onError([](ota_error_t error) {
        logMsg(String(F("OTA error: ")) + error);
      });
      ArduinoOTA.begin();
      otaEnabled = true;
      logMsg(F("OTA habilitado."));
    }
  }
}

static void cmdLogto(const String& params) {
  String p = params; p.trim();
  if (p == F("on")) {
    config.logToRsyslog = true;
    rsyslog.enabled = true;
    logMsg(F("Log para rsyslog ativado."));
    // Also enable rsyslog to ensure clean state
    enableRsyslog();
  } else if (p == F("off")) {
    config.logToRsyslog = false;
    rsyslog.enabled = false;
    logMsg(F("Log para rsyslog desativado."));
  } else {
    logMsg(F("Opção inválida para logto. Use ':cmd logto on' ou ':cmd logto off'."));
    return;
  }
  // salvar após mudar
  extern void saveConfig();
  saveConfig();
}

static void cmdRsyslog(const String& params) {
  String p = params; p.trim();

  if (p == F("enable")) {
    enableRsyslog();
    return;
  }

  // debug toggle: ':cmd rsyslog debug on' or 'debug off'
  if (p.startsWith(F("debug"))) {
    String sub = p.substring(5); sub.trim();
    if (sub == F("on") || sub == F("enable")) {
      setRsyslogDebug(true);
      return;
    } else if (sub == F("off") || sub == F("disable")) {
      setRsyslogDebug(false);
      return;
    } else {
      logMsg(F("Uso: :cmd rsyslog <ip_or_host> | enable | debug on|off"));
      return;
    }
  }

  // If not enable command, treat as server address update
  if (p.length() == 0) {
    logMsg(F("Uso: :cmd rsyslog <ip_or_host> | enable"));
    return;
  }

  config.rsyslogServer = p;
  extern void saveConfig();
  saveConfig();
  
  // Also re-enable rsyslog to apply the new server immediately
  enableRsyslog();
  
  logMsg(String(F("Servidor rsyslog configurado para: ")) + config.rsyslogServer);
}

static void cmdReset(const String&) {
  prefs.begin("kbdcfg", false);
  prefs.clear();
  prefs.end();
  config.logToRsyslog = false;
  config.rsyslogServer = "192.168.5.2";
  config.hostname = "esp32kbd";
  extern void saveConfig();
  saveConfig();
  logMsg(F("Configurações reiniciadas para o padrão."));
}

static void cmdUsbDetach(const String&) {
  if (usbAttached) {
    safeKeyboardEnd();
    usbAttached = false;
    logMsg(F("USB HID desconectado (DETACH)."));
  } else {
    logMsg(F("USB já está desconectado."));
  }
}

static void cmdUsbAttach(const String&) {
  if (!usbAttached) {
#if defined(USB)
    USB.begin();
#endif
    safeKeyboardBegin();
    usbAttached = true;
    logMsg(F("USB HID reconectado (ATTACH)."));
  } else {
    logMsg(F("USB já está conectado."));
  }
}

static void cmdUsbToggle(const String&) {
  if (usbAttached) {
    safeKeyboardEnd();
    usbAttached = false;
    logMsg(F("USB HID toggled -> DISCONNECTED."));
  } else {
#if defined(USB)
    USB.begin();
#endif
    safeKeyboardBegin();
    usbAttached = true;
    logMsg(F("USB HID toggled -> CONNECTED."));
  }
}

static void cmdKeyDelay(const String& params) {
  String msStr = params; msStr.trim();
  int ms = msStr.toInt();
  if (ms >= 0 && ms <= 1000) {
    config.keyDelayMs = ms;
    extern void saveConfig();
    saveConfig();
    logMsg(String(F("Delay entre teclas ajustado para: ")) + ms + F(" ms"));
  } else {
    logMsg(F("Valor inválido para delay. Use entre 0 e 1000 ms."));
  }
}

static void cmdHostname(const String& params) {
  String newHostname = params; newHostname.trim();
  if (newHostname.length() > 0 && newHostname.length() <= 32) {
    config.hostname = newHostname;
    WiFi.setHostname(config.hostname.c_str());
    extern void saveConfig();
    saveConfig();
    logMsg(String(F("Hostname configurado para: ")) + config.hostname);
    logMsg(F("Reinicie o dispositivo para aplicar completamente as alterações de hostname"));
  } else {
    logMsg(F("Hostname inválido. Use entre 1 e 32 caracteres."));
  }
}

void initCommands() {
  // Comandos :cmd <...>
  commandTable["status"]    = [](const String&){ cmdStatus(""); };
  commandTable["reboot"]    = [](const String&){ cmdReboot(""); };
  commandTable["help"]      = [](const String&){ cmdHelp(""); };
  commandTable["logto"]     = cmdLogto;
  commandTable["rsyslog"]   = cmdRsyslog;
  commandTable["reset"]     = [](const String&){ cmdReset(""); };
  commandTable["usb"]       = [](const String& p) {
    String sub = p; sub.trim();
    if (sub == F("detach")) cmdUsbDetach("");
    else if (sub == F("attach")) cmdUsbAttach("");
    else if (sub == F("toggle")) cmdUsbToggle("");
    else logMsg(F("Uso: :cmd usb detach|attach|toggle"));
  };
  commandTable["keydelay"]  = cmdKeyDelay;
  commandTable["hostname"]  = cmdHostname;
  commandTable["ota"]       = cmdOta;
  // Disconnect client aliases
  commandTable["exit"] = [](const String&){ if (client && client.connected()) { logMsg(F("Desconectando cliente TCP (exit)...")); client.stop(); } else { logMsg(F("Nenhum cliente TCP conectado.")); } };
  commandTable["sair"] = commandTable["exit"];
  commandTable["quit"] = commandTable["exit"];
}

void processCommand(const String &incoming) {
  // Preserve o texto original para digitação (mantendo maiúsculas)
  String original = incoming;
  original.trim();

  // Use uma cópia normalizada apenas para detecção de comandos
  String normalized = normalizeCommand(incoming);
  normalized.trim();

  // :press (detecção insensível a maiúsc/minusc)
  if (normalized.startsWith(F(":press "))) {
    String keyStr = normalized.substring(7);
    handlePressCommand(keyStr);
    return;
  }

  // :type (respeitar o texto original após o prefixo)
  if (normalized.startsWith(F(":type "))) {
    String textToType = original;
    // suportar várias formas de prefixo que o usuário pode enviar:
    // ":type <text>", ":TYPE <text>", "type:<text>" e "TYPE:<text>"
    if (textToType.startsWith(F(":type ")) || textToType.startsWith(F(":TYPE "))) {
      // remove ":type " (6 chars)
      textToType = textToType.substring(6);
    } else if (textToType.startsWith(F("type:")) || textToType.startsWith(F("TYPE:"))) {
      // legacy form without leading colon: "type:<text>" -> remove 5 chars
      textToType = textToType.substring(5);
    } else {
      // fallback: if original shorter, use empty
      if (original.length() > 6) textToType = original.substring(6);
      else textToType = String();
    }
    textToType.trim();
    // Log the text being typed BEFORE sending it to the HID layer so the
    // serial/TCP/rsyslog output shows the high-level action first. This
    // prevents per-character/special-key logs (like "Pressionado: ENTER")
    // from appearing before the overall "Digitando" message.
    logMsg(String(F("Digitando: ")) + textToType);
    processAndType(textToType);
    return;
  }

  // Default: qualquer texto que não seja :cmd deve ser digitado (usar original para manter case)
  if (!normalized.startsWith(F(":cmd"))) {
    String textToType = original;
    // See note above: log the high-level action before issuing individual
    // keypresses so logs remain readable and in intuitive order.
    logMsg(String(F("Digitando: ")) + textToType);
    processAndType(textToType);
    return;
  }

  // Remover prefixo ":cmd" (usando normalized para chave sensível a case)
  String arg = normalized.substring(4);
  arg.trim();

  // Separar chave e parâmetros
  int space = arg.indexOf(' ');
  String key = (space > 0) ? arg.substring(0, space) : arg;
  String params = (space > 0) ? arg.substring(space+1) : "";

  auto it = commandTable.find(key);
  if (it != commandTable.end()) {
    it->second(params);
  } else {
    logMsg(String(F("Comando desconhecido: ")) + key);
    logMsg(F("Use ':cmd help' para ver opções."));
  }
}