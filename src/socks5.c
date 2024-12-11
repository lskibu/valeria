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
	int val=0;
	socklen_t len = 0;
	if(!conn->open) {
		DEBUG("Oops! not open");
        return; 
	}
    if(flags&EPOLLERR || flags&EPOLLRDHUP || flags&EPOLLHUP) {
        DEBUG("handle_client: epoll event reporeted an error\n");
		if(conn->type == TARGET) {
			getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &val, &len);
			switch(val) {
				case ECONNRESET:
				case ECONNREFUSED: 
					send_reply(&conn->srv->connections[conn->dst_fd] , REPLY_REFUSED);
					break;
				case EHOSTDOWN:
				case EHOSTUNREACH:
					send_reply(&conn->srv->connections[conn->dst_fd] , REPLY_HSTNRCH);
					break;
				case ENETDOWN:
				case ENETRESET:
				case ENETUNREACH:
					send_reply(&conn->srv->connections[conn->dst_fd] , REPLY_NETNRCH);
					break;
				default: 
				send_reply(&conn->srv->connections[conn->dst_fd] , REPLY_FAILURE);
				break;
			}
			DEBUG("Failed to connect to target");
			connection_close(&conn->srv->connections[conn->dst_fd]);
		}
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
		if(flags & EPOLLOUT) {
			if(errno == EINPROGRESS) {
				errno = 0;
				DEBUG("In Progress");
			} else {
				DEBUG("Target connection success");
				send_reply(&conn->srv->connections[conn->dst_fd], REPLY_SUCCESS);	
				conn->srv->connections[conn->dst_fd].state = S5_CONNECT;
				struct epoll_event ev;
				ev.events = EPOLLIN;
				ev.data.fd = conn->fd;
				if(epoll_ctl(conn->srv->epollfd, EPOLL_CTL_MOD, conn->fd, &ev) < 0)
					DEBUG("epoll_ctl failed");
			}

		} else if (flags & EPOLLIN) {
			if(proxy_data(conn) < 0)
				DEBUG("proxy_data failed");
		}

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
	
	conn->recv_time = time(NULL);

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
	struct sockaddr_in target_addr;
	struct sockaddr_in6 target_addr6;
	struct socks5_request_msg msg;
	unsigned int len, reply;
	int addr_type = AF_INET;
	in_addr_t dst_addr = INADDR_ANY;
	in_port_t dst_port = 0;
	struct in6_addr dst_addr6 = in6addr_any;
	struct hostent *dnsinfo = NULL;
	char hostname[256]={0};
	int fd;

	DEBUG("Processing request...");

	memset(&msg, 0, sizeof msg);
	
	len = recv(conn->fd, &msg, sizeof msg, MSG_DONTWAIT);
	
	if(len < 0)
		return -1;
#define REPLY_ERR(code) { \
			send_reply(conn, REPLY_ADDRERR); \
            connection_close(conn); \
            return 0; \
	}
	conn->recv_time = time(NULL);

	DEBUG("REQUEST: ver: %d, CMD: %d, ATYP: %d", msg.version,
		msg.command, msg.addr_type);

	if(msg.command != CMD_CONNECT) 
		REPLY_ERR(REPLY_CMDNSPR);

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
					DEBUG("hostent: name: %s, type: %d, length: %d, ip_addr = %s\n",
                dnsinfo->h_name, dnsinfo->h_addrtype, dnsinfo->h_length, inet_ntoa(*(struct in_addr *) dnsinfo->h_addr));
					addr_type = dnsinfo->h_addrtype;
					dst_addr = ((struct in_addr *) dnsinfo->h_addr)->s_addr;
				} else 
					REPLY_ERR(REPLY_HSTNRCH);
				
				dst_port = *((in_port_t *) &msg.buffer[len+1]);
			}
			break;
		case ATYP_IPV6: 
			addr_type = AF_INET6;
			dst_addr6 = ((struct socks5_request_in6_msg *) &msg)->ipv6_addr;
			dst_port = ((struct socks5_request_in6_msg *) &msg)->port;
			break;
		default: 
			DEBUG("Unkown address type");
			REPLY_ERR(REPLY_ADDRERR);
	}

	DEBUG("Request processed! addr type: %d", addr_type);

	if(conn->srv->open_count >= conn->srv->open_max)
		REPLY_ERR(REPLY_FAILURE);

	// try to connect to dst
	fd = socket(addr_type, SOCK_STREAM|SOCK_NONBLOCK, 0);

	if(fd < 0) 
		REPLY_ERR(REPLY_FAILURE);
	
	memset(&target_addr, 0, sizeof target_addr);
	memset(&target_addr6 , 0, sizeof target_addr6);

	if(addr_type==AF_INET) {
		target_addr.sin_family = AF_INET;
		target_addr.sin_addr.s_addr = dst_addr;
		target_addr.sin_port = dst_port;
		
		DEBUG("Connecting to ipv4 address: %s:%d", inet_ntoa(target_addr.sin_addr), ntohs(dst_port));
		
		len = connect(fd, (struct sockaddr *) &target_addr, sizeof target_addr);
	} else {
		memset(hostname, 0, sizeof hostname);
		inet_ntop(AF_INET6, &dst_addr6, hostname, sizeof hostname);
		
		target_addr6.sin6_family = AF_INET6;
		target_addr6.sin6_addr = dst_addr6;
		target_addr6.sin6_port = dst_port;
	
		DEBUG("Connecting to ipv6 address: %s:%d", hostname, ntohs(dst_port));

		len = connect(fd, (struct sockaddr *) &target_addr6, sizeof target_addr6);
	}
	if(errno != EINPROGRESS) {
		DEBUG("Connection failed");
		REPLY_ERR(REPLY_FAILURE);
	}

	connection_open(&conn->srv->connections[fd], TARGET);
	

	conn->dst_fd = fd;
	conn->srv->connections[fd].dst_fd = conn->fd;

	ev.events = EPOLLOUT;
	ev.data.fd = fd;

	if(epoll_ctl(conn->srv->epollfd, EPOLL_CTL_ADD, fd, &ev) < 0)
		return -1;
	
	conn->state = -1;
#undef REPLY_ERR
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

int send_reply(struct connection *conn, int reply)
{
	struct socks5_reply_msg msg;
	int len;

	memset(&msg, 0, sizeof msg);
	msg.version = SOCKS5_VERSION;
    msg.addr_type = 1;
    msg.bind_addr = conn->srv->ip;
    msg.bind_port = conn->srv->port;
	msg.reply = reply;

	len = send(conn->fd, &msg, sizeof msg, MSG_DONTWAIT);

	if(len < 0)
		return -1;

	return 0;
}

