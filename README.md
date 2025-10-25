# ESP32 USB HID Keyboard — Controlador TCP Remoto

Sketch para ESP32-S3 que transforma o dispositivo em um teclado USB (HID) controlado por TCP, com recursos avançados de configuração, atualização OTA e controle remoto. O sistema permite digitar texto e enviar combinações de teclas através de uma interface TCP simples e robusta.

## Principais Recursos

### Funcionalidades Básicas
- Emulação de teclado USB usando `USBHIDKeyboard`
- Interface TCP (porta 1234) para controle remoto
- Processamento de texto direto e comandos especiais
- Buffer circular para histórico de comandos
- Proteção contra race conditions usando mutex FreeRTOS

### Sistema de Logging
- Log local via Serial (115200 baud)
- Log remoto via rsyslog (UDP porta 514)
- Controle automático de falhas e reconexão
- Buffer de mensagens recentes para novos clientes

### Configuração e Persistência
- Configurações salvas em memória flash via `Preferences`
- Hostname configurável
- Delay entre teclas ajustável
- Servidor rsyslog configurável
- Reset para valores padrão

### Conectividade e Atualizações
- Gerenciamento WiFi com reconexão automática
- Atualização OTA via comando ou script
- Estado USB HID controlável (attach/detach)
- Monitoramento de status detalhado

## Comandos

### Comandos de Digitação
- `:press TECLA` - Pressiona uma tecla ou combinação
- `:type TEXTO` - Digita o texto fornecido
- Texto sem comando - Digitado diretamente

### Exemplos de Teclas Especiais
- `:press ENTER`
- `:press TAB`
- `:press WIN+R`       (Windows + R)
- `:press CTRL+c`
- `:press ALT+F4`
- `:press SHIFT+A`
- `:press a`          (caractere único)

### Comandos de Configuração
| Comando | Descrição |
|---------|-----------|
| `:cmd ota` | Habilita atualização OTA |
| `:cmd logto on/off` | Ativa/desativa rsyslog |
| `:cmd rsyslog <IP>` | Define servidor rsyslog |
| `:cmd status` | Mostra status detalhado |
| `:cmd reset` | Restaura configurações |
| `:cmd hostname <nome>` | Define hostname |
| `:cmd keydelay <ms>` | Ajusta delay entre teclas |
| `:cmd usb attach/detach/toggle` | Controla USB HID |
| `:cmd reboot` | Reinicia o ESP32 |
| `:cmd help` | Mostra ajuda |

## Envio de Comandos

### Linux/macOS (netcat)
\`\`\`bash
echo ":press WIN+R" | nc <ESP_IP> 1234
\`\`\`

### Windows PowerShell
\`\`\`powershell
$client = New-Object System.Net.Sockets.TcpClient('<ESP_IP>',1234)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.WriteLine(':press WIN+R')
$writer.Flush()
$client.Close()
\`\`\`

### Python Client
O projeto inclui um cliente Python (`python_client.py`) que oferece uma interface de linha de comando para envio de comandos.

## Configuração do Rsyslog

### Servidor Rsyslog
1. Edite `/etc/rsyslog.conf`:
\`\`\`
module(load="imudp")
input(type="imudp" port="514")

# Log separado para o ESP32
if ($fromhost-ip == '<ESP_IP>') then {
  /var/log/esp32_keyboard.log
  stop
}
\`\`\`

### No ESP32
1. Ative o logging: `:cmd logto on`
2. Configure servidor: `:cmd rsyslog <IP>`

## Guia de Início Rápido

1. Carregue o sketch no ESP32-S3
2. Monitore a porta serial (115200 baud)
3. Anote o IP mostrado no monitor
4. Teste com `:press WIN+R`
5. Configure rsyslog se necessário

## Observações Importantes

- Use `:cmd logto off` durante testes iniciais
- Habilite OTA com `:cmd ota` antes de atualizar
- Verifique status com `:cmd status`
- Monitore falhas de rsyslog no log

## Estrutura do Projeto

| Arquivo | Descrição |
|---------|-----------|
| `esp32_keyboard.ino` | Arquivo principal, inicialização e loop |
| `commands.{h,cpp}` | Processamento de comandos |
| `config.{h,cpp}` | Estruturas e configurações |
| `logging.{h,cpp}` | Sistema de logging |
| `usbkbd.{h,cpp}` | Interface USB HID |
| `wifi_mgr.{h,cpp}` | Gerenciamento WiFi |
| `python_client.py` | Cliente Python |
| `version.h` | Informações de versão |

## Funções Arduino Específicas

### Core Arduino
- `USB.begin()`: Inicializa a pilha USB do ESP32
- `Serial.begin(115200)`: Configura comunicação serial
- `Serial.setDebugOutput(true)`: Habilita debug via Serial
- `WiFi.begin(ssid, password)`: Inicia conexão WiFi
- `WiFi.status()`: Verifica estado da conexão WiFi
- `WiFi.localIP()`: Obtém IP atribuído
- `WiFi.RSSI()`: Obtém força do sinal WiFi
- `WiFi.setHostname()`: Define hostname do dispositivo
- `ESP.restart()`: Reinicia o dispositivo
- `ESP.getFreeHeap()`: Obtém memória heap livre
- `ESP.getFreePsram()`: Obtém memória PSRAM livre

### FreeRTOS
- `xSemaphoreCreateMutex()`: Cria mutex para sincronização
- `xSemaphoreTake()`: Obtém lock do mutex
- `xSemaphoreGive()`: Libera lock do mutex
- `vTaskDelay()`: Atraso não-bloqueante em ticks

### USB HID
- `USBHIDKeyboard.begin()`: Inicia modo teclado HID
- `USBHIDKeyboard.press()`: Pressiona uma tecla
- `USBHIDKeyboard.release()`: Solta uma tecla
- `USBHIDKeyboard.releaseAll()`: Solta todas as teclas

### Armazenamento
- `Preferences.begin()`: Inicia acesso à memória flash
- `Preferences.putString()`: Salva string na flash
- `Preferences.getString()`: Lê string da flash
- `Preferences.clear()`: Limpa todas as configurações

---
Este projeto foi atualizado em 24/10/2025 com melhorias na documentação, sistema de logging e proteção de mutex.