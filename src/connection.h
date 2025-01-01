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



#ifndef CONNECTION_H
#define CONNECTION_H

#include <time.h>
#include "server.h"

#define DEFAULT_MAX_OPEN	(8192)

enum connection_type {
	CLIENT,
	TARGET };

struct connection {
	struct server *srv;
	int fd;
	int dst_fd;
	unsigned char open;
	int state;
	int type;
	time_t recv_time;
};

struct connection *connection_new(int);
int connection_open(struct connection *, int type);
int connection_close(struct connection *);
int connection_destroy(struct connection **);

#endif
