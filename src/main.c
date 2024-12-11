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


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

#include "socks5.h"
#include "server.h"
#include "connection.h"
#include "util.h"

#define DIE(msg, func, args, ...) {DEBUG(msg); func(args, ##__VA_ARGS__); exit(EXIT_FAILURE); }

extern char *optarg;
extern int optind, opterr, optopt;

sig_atomic_t interrupt_flag=0;
int debug=0;
int timeout = 20;
time_t uptime;

void usage();
void version();
void daemonize();
void sigint_handle(int);

int main(int argc,char *argv[])
{
	struct option long_opts[] = {{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"daemon", no_argument, NULL, 'D'},
	{"debug", no_argument, NULL, 'd'},
	{"port", required_argument, NULL, 'p'},
	{"address", required_argument, NULL, 'a'}};
	int opt;
	int daemon = 0;
	int long_optind=0;

	struct server *srv;
	char listen_addr[256]="127.0.0.1";
	unsigned short port=1080;

	struct rlimit  rlim;
	int max_open;
	
	while((opt=getopt_long(argc,argv, "hvDdp:a:", long_opts, &long_optind))!=-1) {
		switch(opt) {
			case 'h': 
				usage(); 
				exit(EXIT_SUCCESS);
			case 'v':
				version(); 
				exit(EXIT_SUCCESS);
				break;
			case 'D':
				daemon = 1;
				break;
			case 'd': 
				debug = 1;
				break;
			case 'a': 
				strncpy(listen_addr, optarg, strlen(optarg));
				break;
			case 'p': 
				port = (unsigned short)atol(optarg);
				break;
			default:
				usage();	
				exit(EXIT_FAILURE);
		}
	}
	if(daemon) 
		daemonize();

    if(getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
        DEBUG("getrlimit() failed");
		return -1;
	}
	rlim.rlim_cur = rlim.rlim_max;
	if(setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		DEBUG("setrlimit() failed");
		return -1;
	}

	max_open = (sysconf(_SC_OPEN_MAX) > 0 ? sysconf(_SC_OPEN_MAX) : DEFAULT_MAX_OPEN);
    DEBUG("max_open=%d", max_open);

	if(signal(SIGINT, sigint_handle)==SIG_ERR) 
		DEBUG("signal() failed");

	if((srv = server_create(max_open))==NULL) {
		DEBUG("server_create failed");
		return -1;
	};
	
	if(server_init(srv, listen_addr, port) < 0) 
		DIE("server_init failed", server_destroy, &srv);

	for(int i=0;i < 5; i++)
        connection_open(&srv->connections[i], -1);

	if(server_socket_bind(srv) < 0)
		DIE("server_socket_bind failed", server_destroy, &srv);

	if(server_listen(srv) < 0)
		DIE("server_listen failed", server_destroy, &srv);

	uptime = time(NULL);

	if(server_start(srv) < 0)
		DIE("server_start failed", server_destroy, &srv);

	return 0;
}

void usage()
{
	fprintf(stderr, "Usage: ./valeria [OPTIONS]\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-D,--daemon \t\tDaemonize the server (run in bg)\n");
	fprintf(stderr, "\t-a,--address <addr>\tBind address.(default: localhost)\n");
	fprintf(stderr, "\t-p,--port <port>   \tBind port. (default: 1080)\n");
	fprintf(stderr, "\t-d,--debug  \t\tPrint debug messages (if -D no message is printed)\n");
	fprintf(stderr, "\t-v,--version\t\tPrint program version then exit.\n");
	fprintf(stderr, "\t-h,--help   \t\tPrint this help message then exit\n");
}

void version()
{
	fprintf(stderr, "valeria v0.1 - socks5 server that runs on win32/linux\n");
	fprintf(stderr, "Copyright (C) 2024 lskibu\n");
}


void daemonize()
{
	switch(fork()) {
		case -1:
			fprintf(stderr, "Could not create a child process\n");
			exit(EXIT_FAILURE);
		case 0: break;
		default: _exit(EXIT_SUCCESS);
	}

	if(setsid()==-1)
		_exit(EXIT_FAILURE);
  
	switch(fork()) {
		case -1:
			fprintf(stderr, "Could not create a child process\n");
			exit(EXIT_FAILURE);
		case 0: break;
		default: _exit(EXIT_SUCCESS);
	}

	umask(0);
	if(chdir("/") < 0)
		_exit(EXIT_FAILURE);
	int fd, maxfd = sysconf(_SC_OPEN_MAX);
	maxfd = maxfd > 0 ? maxfd : 1024 * 8;
  
	for(fd=0;fd < maxfd; fd++)
		close(fd);

	fd = open("/dev/null", O_RDWR);
	if(fd!=STDIN_FILENO)
		_exit(EXIT_FAILURE);
	if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
		_exit(EXIT_FAILURE);
	if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
		_exit(EXIT_FAILURE);
} 


void sigint_handle(int sig)
{
	(void) sig;
	interrupt_flag = 1;
}

