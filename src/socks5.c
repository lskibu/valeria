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

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <errno.h>

#include "util.h"
#include "server.h"
#include "connection.h"
#include "socks5.h"

extern int debug;

int socks5_auth_check(struct connection *conn) 
{
	unsigned char buf[512];
	unsigned char data[2];
	char pwd[256];
	char uname[256];
	int ulen, plen;
	int len;
	
	memset(buf, 0, sizeof buf);
	
	len = recv_buf(conn->fd, buf, sizeof buf);
	if(len < 0)
		return -1;
	conn->recv_time = time(NULL);
	ulen = (int) buf[1];
	strncpy(uname, buf+2, ulen);
	plen = (int) buf[2+ulen];
	strncpy(pwd, buf+2+ulen, plen);

	DEBUG("Client auth: %s:%s\n", uname, pwd);
	
	data[0]=SOCKS5_VERSION;
	if(!strcmp(uname, SOCKS5_UNAME) && !strcmp(pwd, SOCKS5_PWD))
		data[1]=0;
	else
		data[1]=255;
	
	len = send(conn->fd, data, sizeof data, MSG_DONTWAIT);
	if(len < 0)
		return -1;

	if(!data[1])
		connection_close(conn);

	conn->state = S5_REQST;
	return 0;
}

void handle_client(struct connection *conn, unsigned int flags)
{
	if(!conn->open) {
		DEBUG("Oops! not open");
        return; 
	}
    if(flags&EPOLLERR || flags&EPOLLRDHUP || flags&EPOLLHUP) {
        DEBUG("handle_client: epoll event reporeted an error\n");
        connection_close(conn);
        return;
    }
	connection_lock(conn);
	if(conn->type==CLIENT) {
		switch(conn->state) {
			case S5_IDENT:
				if(recv_initial_msg(conn) < 0)
					DEBUG("recv_initial_msg failed");
				break;
			case S5_AUTH:
				if(socks5_auth_check(conn) < 0)
					DEBUG("socks5_auth_check failed");
				break;
			case S5_REQST:
				break;
			case S5_REPLY:
				break;
			case S5_CONNECT:
				break;
			case S5_UDPASS:
				break;
			default:
				/* close */
		}
	} else {
		
	}
	connection_unlock(conn);
}

int recv_initial_msg(struct connection *conn)
{
	struct socks5_version_identifier_msg msg;
	struct socks5_method_select_msg msg1;
	int len;
	int use_auth = 0;

	memset(&msg, 0, sizeof msg);
	len = recv(conn->fd, (char *) &msg, sizeof msg, MSG_DONTWAIT);
	
	if(len < 0) 
		return -1;
	DEBUG("SOCKS version: %d", msg.version);
	
	conn->recv_time = time(NULL);
	for(int i=0;i < msg.nmethods; i++)
		if(msg.methods[i]==METHOD_PASSWD) 
			use_auth = 1;

	memset(&msg1, 0, sizeof msg1);
	
	msg1.version = SOCKS5_VERSION;
	if(use_auth)
		msg1.method = METHOD_PASSWD;
	else
		msg1.method = METHOD_NOAUTH;
	
	len = send(conn->fd, (char *)&msg1, sizeof msg1, MSG_DONTWAIT);
	if(len < 0) 
		return -1;
	
	DEBUG("Sent data Length: %d\n", len);
	if(use_auth)
		conn->state = S5_AUTH;
	else
		conn->state = S5_REQST;
	return 0;
}

int process_request(struct connection *conn) 
{
	struct socks5_request_msg msg;
	int len;

	memset(&msg, 0, sizeof msg);
	
	len = recv(conn->fd, &msg, sizeof msg, MSG_DONTWAIT);
	if(len < 0)
		return -1;

	return 0;
}

