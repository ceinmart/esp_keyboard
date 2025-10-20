ESP32 USB HID Keyboard — controlador TCP remoto


Sketch para ESP32-S3 que transforma o dispositivo em um teclado USB (HID) controlado por TCP, agora com recursos avançados de configuração, atualização OTA e controle remoto.

Principais recursos
- Emula teclado USB usando `USBHIDKeyboard`.
- Recebe comandos por TCP (porta 1234 padrão).
- Suporte a comandos flexíveis: `:press`, `:type`, `:cmd` (não sensível a maiúsculas/minúsculas).
- Qualquer texto enviado que não começar com `:press` ou `:cmd` será digitado automaticamente.
- Interpreta `\n` como ENTER e `\\` como barra invertida literal.
- Buffer circular exibe as últimas mensagens ao conectar via TCP.
- Envio de logs para rsyslog via UDP (porta 514), com controle automático de tentativas e desabilitação após falhas.
- Configurações persistentes com `Preferences` (hostname, delay, log, servidor rsyslog).
- Atualização OTA do firmware via comando `:cmd ota` e task automatizada no VS Code (`Enviar OTA (espota.py)`).
- Controle do USB HID via comandos (`:cmd usb attach`, `:cmd usb detach`, `:cmd usb toggle`).
- Ajuste do delay entre teclas via comando (`:cmd keydelay <ms>`).
- Configuração do hostname via comando (`:cmd hostname <nome>`).
- Comando para reboot remoto (`:cmd reboot`).
- Comando para restaurar configurações padrão (`:cmd reset`).
- Comando para mostrar status detalhado (`:cmd status`), incluindo uptime, heap livre, hostname, IP, RSSI, estado do USB HID, estado do rsyslog.
- Mensagens de ajuda detalhadas via `:cmd help`.

Comandos rápidos (exemplos)
- `:press ENTER`
- `:press TAB`
- `:press WIN+R`       (Windows + R)
- `:press CTRL+c`
- `:press ALT+F4`
- `:press SHIFT+A`
- `:press a` (caractere único)


Configuração e comandos principais
- `:cmd ota` — habilita atualização OTA do firmware (use o script espota.py ou a task do VS Code)
- `:cmd logto on` / `:cmd logto off` — ativa/desativa envio de logs para rsyslog
- `:cmd rsyslog <IP_OR_HOST>` — define o servidor rsyslog
- `:cmd status` — mostra status detalhado (IP, hostname, uptime, log, USB, etc)
- `:cmd reset` — restaura padrões (log off, servidor, hostname, delay)
- `:cmd hostname <nome>` — define o hostname do dispositivo
- `:cmd keydelay <ms>` — ajusta o delay entre teclas (ms)
- `:cmd usb attach` / `:cmd usb detach` / `:cmd usb toggle` — controla o estado do USB HID
- `:cmd reboot` — reinicia o ESP32
- `:cmd help` — mostra ajuda com todos comandos disponíveis

echo ":press WIN+R" | nc <ESP_IP> 1234
Exemplos de envio (um comando por conexão)

echo "PRESS:WIN+R" | nc <ESP_IP> 1234

Linux / macOS (netcat):
echo ":press WIN+R" | nc <ESP_IP> 1234

Windows PowerShell:
$client = New-Object System.Net.Sockets.TcpClient('<ESP_IP>',1234)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.WriteLine(':press WIN+R')
$writer.Flush()
$client.Close()

Observações de rede e rsyslog

Notas sobre rsyslog
- O sketch envia logs por UDP (porta 514) para o servidor configurado.
- Habilite `imudp` e a porta 514 no `rsyslog` do destino.
- O log é desabilitado automaticamente após falhas consecutivas na conexão.

Exemplo mínimo (`/etc/rsyslog.conf` moderno):
module(load="imudp")
input(type="imudp" port="514")

# Opcional: gravar mensagens deste host num arquivo separado
if ($fromhost-ip == '<ESP_IP>') then {
  /var/log/esp32_keyboard.log
  stop
}

Testes rápidos
1. Carregue o sketch no ESP32-S3 e abra o monitor serial (115200).
2. Na máquina que enviará comandos, confirme a conectividade TCP com o IP que aparece no Serial.
3. Envie `:press WIN+R` e confirme que o dispositivo emula Windows+R.
4. Ative logs: `:cmd logto on` e defina o servidor: `:cmd rsyslog 192.168.1.50`.

Observações

- Para atualização OTA, habilite com `:cmd ota` e use o script `espota.py` ou a task do VS Code.
- Até testar o HID, mantenha `:cmd logto off` para evitar dependência de rsyslog.
- Se precisar de suporte a caracteres acentuados PT-BR, posso incluir um mapeamento opcional.

Arquivo principal: `esp32_keyboard.ino`

---
Este projeto foi atualizado em 02/10/2025 com novos recursos de configuração, OTA, controle USB e melhorias de comandos. Para dúvidas ou sugestões, abra uma issue ou entre em contato.