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
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#include "util.h"
#include "server.h"
#include "connection.h"
#include "socks5.h"

extern sig_atomic_t interrupt_flag;
extern int debug;
extern int timeout;
extern int uptime;

struct server* server_create(size_t max_open)
{
	struct server *srv = (struct server *) calloc(1, sizeof(struct server));
	if(srv==NULL) return NULL;
	srv->open_max = max_open;
	return srv;
}

int server_init(struct server *srv, char *addr,unsigned short port)
{
	srv->ip = inet_addr(addr);
	srv->port = htons(port);
	srv->connections = (struct connection *) calloc(srv->open_max, sizeof(struct connection));
	if(srv->connections == NULL)
		return -1;
	for(int i=0;i < srv->open_max; i++) {
		srv->connections[i].fd = i;
		srv->connections[i].srv = srv;
	}
	return 0;
}

int server_socket_bind(struct server *srv)
{
	struct sockaddr_in addr;
	int val = 1;
	srv->fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
	if(srv->fd < 0) 
		return -1;
	DEBUG("created server socket");
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = srv->ip;
	addr.sin_port = srv->port;
	
	if(setsockopt(srv->fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &val, sizeof val) < 0)
		return -1;
		
	if(bind(srv->fd, (struct sockaddr *)&addr, sizeof addr) < 0)
		return -1;
	DEBUG("bound server socket");
	srv->epollfd = epoll_create1(0);
	if(srv->epollfd < 0)
		return -1;
	return 0;
}

int server_listen(struct server *srv)
{
	struct epoll_event ev;
	if(listen(srv->fd, 128) < 0)
		return -1;
	ev.events = EPOLLIN;
	ev.data.fd = srv->fd;

	if(epoll_ctl(srv->epollfd, EPOLL_CTL_ADD, srv->fd, &ev) < 0)
		return -1;

	return 0;
}

int server_start(struct server *srv)
{
	struct epoll_event events[1024], event;
	struct sockaddr_in addr;
	int fd;
	socklen_t len;
	
	for(;;) {
		if(interrupt_flag) {
			DEBUG("received Ctrl+c");
			break;
		}
		
		int nfds = epoll_wait(srv->epollfd, events, sizeof events/ sizeof events[0], 1000);

		if(nfds < 0 && errno!=EINTR)
			return -1;

		for(int i=0;i < nfds; i++) {
			
			if(events[i].data.fd==srv->fd) {
				
				if(srv->open_count >= srv->open_max)
					continue;
				
				memset(&addr, 0, sizeof addr);
				fd = accept(srv->fd, (struct sockaddr *)&addr, &len);
				if(fd < 0)
					return -1;
				
				DEBUG("%s:%d connected", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
				connection_open(&srv->connections[fd], CLIENT);
				srv->connections[fd].state = S5_IDENT;
				
				event.events = EPOLLIN|EPOLLRDHUP;
				event.data.fd = fd;
				
				if(epoll_ctl(srv->epollfd, EPOLL_CTL_ADD, fd, &event) < 0)
					return -1;
			}
			else 
				handle_client(&srv->connections[events[i].data.fd], events[i].events);
		}

		server_timeout(srv);

		if(!debug) 
            fprintf(stderr, "UPTIME: %ld secs | OPEN CONNECTIONS: %ld\r",
                time(NULL) - uptime, srv->open_count - 5);
		
	}
	return 0;
}

int server_timeout(struct server *srv) 
{
	for(int i=5;i < srv->open_count; i++)
	{
		if(srv->connections[i].lock || !srv->connections[i].open)
			continue;

		if(time(NULL) - srv->connections[i].recv_time >= timeout) {
			if(srv->connections[i].type == TARGET) {
				if(srv->connections[srv->connections[i].dst_fd].open) {
					send_reply(&srv->connections[srv->connections[i].dst_fd], REPLY_EXPIRED);
					connection_close(&srv->connections[srv->connections[i].dst_fd]);
				}
			}
			DEBUG("CONNECTION TIMEOUT");
			connection_close(&srv->connections[i]);
		}
	}
	return 0;
}

void server_destroy(struct server **srv)
{
	close((*srv)->fd);
	close((*srv)->epollfd); // ignore return values
	free((*srv)->connections);
	(*srv)->connections = NULL;
	free(*srv);
	*srv = NULL;
}

