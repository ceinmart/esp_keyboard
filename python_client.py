
# Arquivo: python_client.py
# Cliente Python para controle do teclado ESP32 via TCP
# Permite enviar comandos de digitação e teclas especiais para o ESP32
# através de uma interface de linha de comando simples.

import socket
import time

# Configurações de conexão
ESP32_IP = "192.168.5.157"  # IP do ESP32 (altere para o IP do seu dispositivo)
ESP32_PORT = 1234           # Porta TCP padrão do serviço

def send_command(command):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ESP32_IP, ESP32_PORT))
            s.sendall(f"{command}\n".encode())
            print(f"Comando enviado: {command}")
    except ConnectionRefusedError:
        print(f"Erro: Conexão recusada. Verifique se o ESP32 está online e o IP/porta estão corretos.")
    except Exception as e:
        print(f"Ocorreu um erro: {e}")

if __name__ == "__main__":
    print("Cliente Python para Teclado WiFi USB com ESP32")
    print("Comandos disponíveis: :type <texto>, :press <tecla> (ex: :press ENTER, :press SHIFT+A)")
    print("Digite 'exit' para sair.")

    while True:
        cmd = input("Digite o comando: ")
        if cmd.lower() == 'exit':
            break
        send_command(cmd)
        time.sleep(0.1) # Pequeno atraso entre comandos

