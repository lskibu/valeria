/**
 * valeria - A socks5 server that runs on linux/Win32
 *
 * Copyright (C) 2024 lskibu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */




#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>


int socket_connect(char *ip, unsigned short port)
{
	struct sockaddr_in addr;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0)
		return -1;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	if(connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0)
		return -1;
	return fd;
}

int send_buf(int fd, char *buf, size_t len)
{
	int n;
send_again:
    n = send(fd, buf, len, MSG_DONTWAIT);
    if(n < 0) {
        if(errno==EAGAIN || errno==EWOULDBLOCK)
            goto send_again;
        else
            return -1;
    }
	return n;
}

int recv_buf(int fd, char *buf, size_t len)
{
	int n;
recv_again:
    n = recv(fd, buf, len, MSG_DONTWAIT);
    if(n < 0) {
        if(errno==EAGAIN || errno==EWOULDBLOCK)
            goto recv_again;
        else
            return -1;
    }
	return n;
}

