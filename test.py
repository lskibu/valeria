import socket
socks=[]
for i in range(500):
	sock=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
	sock.connect(('127.0.0.1', 1080))
	socks.append(sock)
for i in range(10):
	for sock in socks:
		sock.send(b'hello world')
	#while True:
		#ret = sock.send(b'HELLO SERVER!')
		#if ret > 0:
			#break
		print("Received data:", sock.recv(4096))
	#sock.close()

for sock in socks:
	sock.close()

