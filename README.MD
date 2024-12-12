### Valeria v0.2.2

A socks v5 proxy server written in C language for native support
Runs on linux only for now...
It is a personal project that aims to provide a single threaded asynchronous
socks5 proxy server... 
The implementation of this source code is based on rfc1928 and rfc1929

#### Build
Requires make and gcc 
	
	sudo apt install gcc make -y


Build cmd:
	
	git clone https://github.com/lskibu/valeria.git
	cd valeria
	make -j
	./valeria --version


#### TO DO
1. native win32 support
2. TCP BIND and UDP Associate support
3. checks and bug fixes
