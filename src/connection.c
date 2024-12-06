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

#include <stdlib.h>
#include <time.h>
#include "connection.h"
#include "util.h"


struct connection *connection_new(int fd)
{
	struct connection *conn = (struct connection *) calloc(1, sizeof(struct connection));
	if(!conn) 
		return NULL;
	conn->fd = fd;
	return conn;
}

int connection_open(struct connection *conn,int type)
{
	conn->open = 1;
	conn->type = type;
	conn->recv_time= time(NULL);
	ATOMIC_INC(&conn->srv->open_count);
	return 0;
}

void connection_lock(struct connection *conn) 
{
	SYNC_LOCK(&conn->lock);
}

void connection_unlock(struct connection *conn)
{
	SYNC_UNLOCK(&conn->lock);
}
int connection_is_locked(struct connection *conn)
{
	return SYNC_IS_LOCKED(&conn->lock);
}

int connection_close(struct connection *conn)
{
	close(conn->fd);
	conn->open = 0;
	ATOMIC_DEC(&conn->srv->open_count);
	return 0;
}
int connection_destroy(struct connection **conn)
{
	free(*conn);
	*conn = NULL;
	return 0;
}
