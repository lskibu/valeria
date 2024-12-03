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

#include "connection.h"

struct server {
	int fd;
	int epoll_fd;
	size_t connection_open_count;
	size_t connection_open_max;
	struct connection *connections;
};

struct server* server_create(size_t );
int server_socket_bind(struct server *);
int server_listen(struct server *);
void server_destroy(struct server **);


#endif
