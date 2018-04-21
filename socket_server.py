import numpy
import socket
import sounddevice as sd
"""
THIS IS FOR UDP RECEIVING
"""
fs = 44100
UDP_IP = "127.0.0.1"
UDP_PORT = 50001
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

s.bind((UDP_IP, UDP_PORT))

while True:
    print("Receiving")
    data, addr = s.recvfrom(1024)  # buffer size is 1024 bytes
    print(data)
    #sd.play(data, fs)