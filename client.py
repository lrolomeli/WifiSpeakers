import socket
from scipy.io import wavfile
import time
#import sounddevice as sd
"""
THIS IS FOR UDP SENDING
"""
UDP_IP = "192.168.0.102"
UDP_PORT = 50001

print("UDP target IP: ", UDP_IP)
print("UDP target port: ", UDP_PORT)

fs, data = wavfile.read('NintendoWii.wav')
"""
data = (stereo[:, 0] + stereo[:, 1])
"""
data = (data[:, 0])
data = data >> 4
data = data + 2047
#sd.play(data, fs)
#sd.wait()
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sample_size = 400
i = 0
j = 400

buffer = data[i:j]

while len(buffer) != 0:
    s.sendto(buffer, (UDP_IP, UDP_PORT))
    i = i + sample_size
    j = j + sample_size
    buffer = data[i:j]
    time.sleep(0.0092)
