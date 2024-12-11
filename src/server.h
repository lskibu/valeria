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



#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include "connection.h"

struct server {
	int fd;
	int epollfd;
	in_addr_t ip;
	in_port_t port;
	size_t open_count;
	size_t open_max;
	struct connection *connections;
};

struct server* server_create(size_t );
int server_init(struct server *, char *,unsigned short );
int server_socket_bind(struct server *);
int server_listen(struct server *);
int server_start(struct server *);
int server_timeout(struct server *);
void server_destroy(struct server **);

#endif
