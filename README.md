ESP32 USB HID Keyboard — controlador TCP remoto

Sketch para ESP32-S3 que transforma o dispositivo em um teclado USB (HID) controlado por TCP.

Principais recursos
- Emula teclado USB usando `USBHIDKeyboard`.
- Recebe comandos por TCP (porta 1234 padrão).
- Suporta `PRESS:` para teclas e combinações (CTRL, ALT, SHIFT, WIN).
- Qualquer texto enviado que não começar com `PRESS:` ou `CMD:` será digitado automaticamente.
- Interpreta `\n` como ENTER e `\\` como barra invertida literal.
- Envio de logs para rsyslog via UDP (porta 514), opcional.
- Configurações persistentes com `Preferences` (mudanças sem recompilar).

Comandos rápidos (exemplos)
- `PRESS:ENTER`
- `PRESS:TAB`
- `PRESS:WIN+R`       (Windows + R)
- `PRESS:CTRL+c`
- `PRESS:ALT+F4`
- `PRESS:SHIFT+A`
- `PRESS:a` (caractere único)

Configuração de log (rsyslog)
- `CMD:LOGTO:on`     — ativa envio de logs
- `CMD:LOGTO:off`    — desativa envio
- `CMD:RSYSLOG:<IP_OR_HOST>`  — define o servidor rsyslog (ex: `CMD:RSYSLOG:192.168.1.50`)
- `CMD:STATUS`       — mostra status atual (IP, estado do log, servidor)
- `CMD:RESET`        — restaura padrões (log off, servidor `192.168.1.100`)

echo "PRESS:WIN+R" | nc <ESP_IP> 1234
Exemplos de envio (um comando por conexão)

Linux / macOS (netcat):

echo "PRESS:WIN+R" | nc <ESP_IP> 1234

Windows PowerShell (exemplo simples):

$client = New-Object System.Net.Sockets.TcpClient('<ESP_IP>',1234)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.WriteLine('PRESS:WIN+R')
$writer.Flush()
$client.Close()

Observações de rede e rsyslog
Notas sobre rsyslog
- O sketch envia logs por UDP (porta 514) para o servidor configurado.
- Habilite `imudp` e a porta 514 no `rsyslog` do destino.

Exemplo mínimo (`/etc/rsyslog.conf` moderno):

module(load="imudp")
input(type="imudp" port="514")

# Opcional: gravar mensagens deste host num arquivo separado
if ($fromhost-ip == '192.168.1.100') then {
  /var/log/esp32_keyboard.log
  stop
}

Testes rápidos
1. Carregue o sketch no ESP32-S3 e abra o monitor serial (115200).
2. Na máquina que enviará comandos, confirme a conectividade TCP com o IP que aparece no Serial.
3. Envie `PRESS:WIN+R` e confirme que o dispositivo emula Windows+R.
4. Ative logs: `CMD:LOGTO:on` e defina o servidor: `CMD:RSYSLOG:192.168.1.50`.

Observações
- Até testar o HID, mantenha `CMD:LOGTO:off` para evitar dependência de rsyslog.
- Se precisar de suporte a caracteres acentuados PT-BR, posso incluir um mapeamento opcional.

Arquivo principal: `esp32_keyboard.ino`

--
Escrito e organizado para facilitar testes. Posso ajustar exemplos ou adicionar uma seção de troubleshooting se desejar.