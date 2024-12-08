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
#include <netdb.h>
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
	
	len = recv (conn->fd, buf, sizeof buf, MSG_DONTWAIT);

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
				if(process_request(conn) < 0)
					DEBUG("process_request failed");
				break;
			case S5_CONNECT:
				if(proxy_data(conn) < 0)
					DEBUG("proxy_data failed");
				break;
			case S5_UDPASS:
				break;
			default:
				/* close */
		}
	} else {
		if(proxy_data(conn) < 0)
            DEBUG("proxy_data failed");
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
	struct epoll_event ev;
	struct socks5_request_msg msg;
	unsigned int len, reply;
	int addr_type = AF_INET;
	in_addr_t dst_addr = INADDR_ANY;
	in_port_t dst_port = 0;
	struct in6_addr dst_addr6 = IN6ADDR_ANY_INIT;
	struct hostent *dnsinfo = NULL;
	char hostname[256]={0};
	int fd;

#define REPLY_ERROR(flag) do {\
		msg1.reply = flag;\
        len = send(conn->fd, &msg1, sizeof msg1, MSG_DONTWAIT); \
        if(len < 0) \
            return -1; \
        DEBUG("Terminating the connection"); \
        connection_close(conn); \
        return 0; \
    } while(0) ;


	struct socks5_reply_msg msg1;
	
	DEBUG("Processing request...");

	memset(&msg1, 0, sizeof msg1);
	memset(&msg, 0, sizeof msg);
	
	len = recv(conn->fd, &msg, sizeof msg, MSG_DONTWAIT);
	
	if(len < 0)
		return -1;
	
	conn->recv_time = time(NULL);

	DEBUG("REQUEST: ver: %d, CMD: %d, ATYP: %d", msg.version,
		msg.command, msg.addr_type);

	msg1.version = SOCKS5_VERSION;
	msg1.addr_type = 1;
	msg1.bind_addr = conn->srv->ip;
	msg1.bind_port = conn->srv->port;

	if(msg.command != CMD_CONNECT) 
		REPLY_ERROR(REPLY_CMDNSPR);

	switch(msg.addr_type) {
		case ATYP_IPV4:
			addr_type = AF_INET;
			dst_addr = ((struct socks5_request_in_msg *)&msg)->ipv4_addr;
			dst_port = ((struct socks5_request_in_msg *)&msg)->port;
			break;
		case ATYP_NAME: 
			len = (unsigned int) msg.buffer[0];
			if(len < 256) {
				strncpy(hostname, &msg.buffer[1], len);
				dnsinfo = gethostbyname(hostname);
				if(dnsinfo!=NULL && dnsinfo->h_length > 0) {
					DEBUG("hostent: name: %s, type: %d, length: %d\n",
                dnsinfo->h_name, dnsinfo->h_addrtype, dnsinfo->h_length);
					addr_type = dnsinfo->h_addrtype;
					dst_addr = inet_addr(dnsinfo->h_addr_list[0]);
				} else
					REPLY_ERROR(REPLY_HSTNRCH);
				dst_port = *((in_port_t *) &msg.buffer[len+1]);
			}
			break;
		case ATYP_IPV6: 
			addr_type = AF_INET6;
			dst_addr6 = ((struct socks5_request_in6_msg *) &msg)->ipv6_addr;
			dst_port = ((struct socks5_request_in6_msg *) &msg)->port;
			break;
		default: break;
	}
	DEBUG("Request processed! addr type: %d", addr_type);

	// try to connect to dst
	fd = socket(addr_type, SOCK_STREAM|SOCK_NONBLOCK, 0);

	if(fd < 0) 
		switch (errno) {
			case ENETUNREACH: REPLY_ERROR(REPLY_NETNRCH);
			case EHOSTDOWN: REPLY_ERROR(REPLY_HSTNRCH);
			case ECONNREFUSED: REPLY_ERROR(REPLY_REFUSED);
			default: REPLY_ERROR(REPLY_REFUSED);
		}

	DEBUG("SUCCESSFULLY CONNEECTED TO DST ADDR");

	connection_open(&conn->srv->connections[fd], TARGET);
	
	msg1.reply = REPLY_SUCCESS;

    len = send(conn->fd, &msg1, sizeof msg1, MSG_DONTWAIT); 
    if(len < 0) 
        return -1;
	
	conn->dst_fd = fd;
	conn->srv->connections[fd].dst_fd = conn->fd;

	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if(epoll_ctl(conn->srv->epollfd, EPOLL_CTL_ADD, fd, &ev) < 0)
		return -1;
	
	conn->state = S5_CONNECT;
#undef REPLY_ERROR
	return 0;
}

int proxy_data(struct connection *conn)
{
	unsigned char buffer[4096];
	int len = sizeof buffer;
	if(conn->type != TARGET && conn->state != S5_CONNECT) {
		DEBUG("Oops! proxy_data got invalid client");
		connection_close(conn);
		return 0;
	}
	while(len==sizeof buffer) {
		memset(buffer, 0, sizeof buffer);
		len = recv(conn->fd, buffer, sizeof buffer, MSG_DONTWAIT);
		if(len < 0) {
			DEBUG("proxy_data failed while recv()");
			connection_close(conn);
			return -1;
		}

		conn->recv_time = time(NULL);

		len = send(conn->dst_fd, buffer, len, MSG_DONTWAIT);
		if(len < 0) {
            DEBUG("proxy_data failed while send()");
            connection_close(conn);
            return -1;
        }
	}
}

