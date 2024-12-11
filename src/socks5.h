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


#ifndef SOCKS5_H
#define SOCKS5_H

#include <netinet/in.h>
#include "connection.h"

#define SOCKS5_VERSION  (5)

#define SOCKS5_UNAME	"USER"
#define SOCKS5_PWD		"PASS"


enum socks5_state {
	S5_IDENT,
	S5_AUTH,
	S5_REQST,
	S5_REPLY,
	S5_CONNECT,
	S5_UDPASS
};	

enum socks5_auth_method {
	METHOD_NOAUTH = 0, //NO AUTHENTICATION REQUIRED
    METHOD_GSSAPI = 1, //GSSAPI
    METHOD_PASSWD = 2, //USERNAME/PASSWORD
    METHOD_NOACCPT = 255 // NO ACCEPTABLE METHOD
};

enum  socks5_reply {
	REPLY_SUCCESS=0, //succeeded
    REPLY_FAILURE=1, //general SOCKS server failure
    REPLY_NALLOWD=2, //connection not allowed by ruleset
    REPLY_NETNRCH=3, //Network unreachable
    REPLY_HSTNRCH=4, //Host unreachable
    REPLY_REFUSED=5, //Connection refused
    REPLY_EXPIRED=6, //TTL expired
    REPLY_CMDNSPR=7, //Command not supported
    REPLY_ADDRERR=8  //Address type not supported
};

enum socks5_addr_type {
	ATYP_IPV4 = 1,  //o  IP V4 address: X'01'
    ATYP_NAME = 3,  //o  DOMAINNAME: X'03'
    ATYP_IPV6 = 4  //o  IP V6 address: X'04'
};

enum socks5_command {
	CMD_CONNECT=1,
    CMD_BIND=2,
    CMD_UDP_ASSOCIATE=3
};

struct socks5_version_identifier_msg {
	unsigned char version;
	unsigned char nmethods;
	unsigned char methods[256];
} __attribute__((__packed__));

struct socks5_method_select_msg {
	unsigned char version;
	unsigned char method;
};

struct socks5_request_msg {
	unsigned char version;
	unsigned char command;
	unsigned char reserved;
	unsigned char addr_type;
	unsigned char buffer[512];
} __attribute__((__packed__));

struct socks5_request_in_msg {
	unsigned char version;
	unsigned char command;
	unsigned char reserved;
	unsigned char addr_type;
	in_addr_t ipv4_addr;
	in_port_t port;
 } __attribute__((__packed__));

struct socks5_request_in6_msg {
	unsigned char version;
	unsigned char command;
	unsigned char reserved;
	unsigned char addr_type;
	struct in6_addr ipv6_addr;
	in_port_t port;
} __attribute__((__packed__));


struct socks5_reply_msg {
	unsigned char version;
	unsigned char reply;
	unsigned char reserved;
	unsigned char addr_type;
	in_addr_t bind_addr;
	in_port_t bind_port;
}__attribute__((__packed__));

struct socks5_status_msg {
	unsigned char version;
	unsigned char status;
}__attribute__((__packed__));

int socks5_auth_check(struct connection *);
void handle_client(struct connection *, unsigned int);
int recv_initial_msg(struct connection *);
int process_request(struct connection *);
int send_reply(struct connection *, int);
int proxy_data(struct connection *);

#endif 
