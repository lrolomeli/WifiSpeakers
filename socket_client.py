import soundfile as sf
import socket

"""
import numpy
import sounddevice as sd
}

#data, samplerate = sf.read('spring_HiFi.wav')

print(data)
print(len(data))
sd.play(data, samplerate)

sd.wait()

sd.stop()

x = list(data)

print(len(x))

for i in range(len(x)):
    print(x[i])


#PORT SCANNER

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server = 'google.com'

def pscan(port):
    try:
        s.connect((server, port))
        return True
    except:
        return False

for x in range(1, 26):
    if pscan(x):
        print('Port',x,'is open!!!')
    else:
        print('Port',x,'is closed')


#TCP CONNECTION
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

print(s)

server = 'google.com'
port = 80
server_ip = socket.gethostbyname(server)

print(server_ip)

request = "GET / HTTP/1.1\nHost: "+server+"\n\n"

s.connect((server, port))
s.send(request.encode())
result = s.recv(4096)

while len(result) > 0:
    print(result)
    result = s.recv(4096)

#EXAMPLE_OF_CLASS_AND_OBJECT_IN_PYTHON

class UDP_Messages:
    def __init__(self, msg):
        self.msg = msg

m = UDP_Messages("Hello, World")

print(m.msg)
"""

"""
THIS IS FOR UDP SENDING
"""
UDP_IP = "192.168.0.102"
UDP_PORT = 50001
#MESSAGE = "Hello, World!".encode()

print("UDP target IP: ", UDP_IP)
print("UDP target port: ", UDP_PORT)
#print("message: ", MESSAGE)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

buffer, fs = sf.read('spring_HiFi.wav', 512, 0, None, 'int16')
print(buffer)
index_buf = 0
while (len(buffer)) == 512:
    buffer, fs = sf.read('spring_HiFi.wav', 512, index_buf, None, 'int16')
    index_buf += 512
    s.sendto(buffer, (UDP_IP, UDP_PORT))
    print(buffer)
