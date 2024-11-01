import socket

for i in range(5000):
  sock=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
  sock.connect(('127.0.0.1', 1080));
  sock.send(b'HELLO SERVER!')
  sock.close()

