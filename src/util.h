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



#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define SYNC_LOCK(ptr) __sync_lock_test_and_set((ptr),1)
#define SYNC_UNLOCK(ptr) __sync_lock_test_and_set((ptr),0)
#define SYNC_IS_LOCKED(ptr) ATOMIC_GET((ptr))

#define ATOMIC_ADD(ptr,i) __sync_fetch_and_add((ptr),i)
#define ATOMIC_SUB(ptr,i) __sync_fetch_and_sub((ptr),i)
#define ATOMIC_INC(ptr) ATOMIC_ADD((ptr),1)
#define ATOMIC_DEC(ptr) ATOMIC_SUB((ptr),1)
#define ATOMIC_GET(ptr) ATOMIC_ADD((ptr),0)

#define DEBUG(msg, ...)	{\
	if(debug) { \
		if(errno) { \
			fprintf(stderr, "%s:%s:%d - "msg": fatal error: %s\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__, strerror(errno)); \
			errno=0;\
		} \
		else \
			fprintf(stderr, msg, ##__VA_ARGS__); \
		fputc('\n', stderr); }}

int socket_connect(char *ip, unsigned short port);
int send_buf(int fd, char *buf, size_t len);
int recv_buf(int fd, char *buf, size_t len);

#endif
