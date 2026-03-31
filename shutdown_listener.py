# shutdown_listener.py - Run this on your PC
import socket
import os
import platform

UDP_IP = "0.0.0.0"
UDP_PORT = 7777

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening for shutdown commands on port {UDP_PORT}...")

while True:
    data, addr = sock.recvfrom(1024)
    print(f"Received from {addr}: {data.decode()}")
    if data.decode() == "SHUTDOWN_NOW":
        print("Shutdown command received!")
        if platform.system() == "Windows":
            os.system("shutdown /s /t 30 /c 'Shutdown via ESP32'")
        else:
            os.system("shutdown -h +1")
