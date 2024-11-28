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
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>

#include "socks5.h"

#define MAX_OPEN (8192)

#define TYPE_CLI (1)
#define TYPE_TRGT (2)

extern char *optarg;
extern int optind, opterr, optopt;

/* connection info records
 * connection[fd][0]=OPEN|CLOSE
 * connection[fd][1]=TYPE{client, target}
 * connection[fd][2]=STATE
 * connection[fd][3]=TARGETFD
 * connection[fd][4]={last receive time}
 * connection[fd][5]=FLAG: BUSY/READY
*/

static long int connections[MAX_OPEN][6];
static int open_connection_count=0;
static int max_open=0;
static int server_timeout=20;
int serverfd=-1, epollfd=-1;
static sig_atomic_t interrupt=0;

int sock_send(int, unsigned char*, size_t);
int sock_recv(int, unsigned char*, size_t);
void connection_close(int);

void init_connections();
void cleanup();
void usage();
void version();
void daemonize();
void sigint_handle(int);
void handle_event(struct epoll_event *);
void *timeout_proc(void *);

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
	int debug  = 0;
	int long_optind=0;

	struct sockaddr_in server_addr;
	socklen_t addrlen;
	struct sockaddr_in cli_addr;
	int clifd;
	char listen_addr[256]="127.0.0.1";
	unsigned short port=1080;
	struct epoll_event ev, events[128];
	max_open = (sysconf(_SC_OPEN_MAX) > 0 ? sysconf(_SC_OPEN_MAX) : MAX_OPEN);
	// decrement max open 
	max_open--; // STDIN_FILENO
	max_open--; // STDOUT_FILENO
	max_open--; // STDERR_FILENO

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
	if(daemon) daemonize();
	if(signal(SIGINT, sigint_handle)==SIG_ERR) {
		fprintf(stderr, "signal failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	// create IPv4 socket, TCP protocol
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if(serverfd==-1) {
		fprintf(stderr, "socket failed: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	max_open--; // socket fd
	// set sock opttion reuseaddr
	socklen_t len=sizeof (int);
	int optval = 1;
	if(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, len) < 0) {
		fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
		goto exit_failure;
	}

	// register addr of socket
	memset(&server_addr, 0, sizeof server_addr);	
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(listen_addr);
	server_addr.sin_port=htons(port);
	if(bind(serverfd, (struct sockaddr *)&server_addr, sizeof server_addr)==-1) {
		fprintf(stderr, "bind failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	// listen for incoming connections
	if(listen(serverfd, 128)==-1) {
		fprintf(stderr, "listen failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	if(fcntl(serverfd, F_SETFD, O_NONBLOCK|fcntl(serverfd, F_GETFD, 0))==-1) {
		fprintf(stderr, "fcntl failed: %s\n", strerror(errno));	
		goto exit_failure;
	}
	
	epollfd = epoll_create1(0);
	if(epollfd==-1) {
		fprintf(stderr, "epoll_create1 failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	max_open--; // epoll fd
	ev.events=EPOLLIN;
	ev.data.fd=serverfd;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &ev)==-1) {
		fprintf(stderr, "epoll_ctl failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	// loop forever
	pthread_t tid;
	if(pthread_create(&tid, NULL, timeout_proc, NULL) < 0) {
		fprintf(stderr, "pthread_create failed: %s\n", strerror(errno));
		goto exit_failure;
	}
	for(;;) {
		// wait until there is an up coming event
		if(interrupt) break; // handle SIGINT
		int nfds = epoll_wait(epollfd, events, sizeof events/sizeof events[0], -1);
		if(nfds==-1) {
			if(errno==EINTR)
				continue;
			fprintf(stderr, "epoll_wait failed: %s\n", strerror(errno));
			goto exit_failure;
		}
		// handle events
		for(int i=0;i < nfds; i++) {
			// upcoming connection
			if(events[i].data.fd==serverfd) {
				if(open_connection_count >= max_open)
					continue;
				memset(&cli_addr, 0, sizeof cli_addr);
				addrlen = sizeof cli_addr;
				clifd = accept(serverfd, (struct sockaddr *)&cli_addr, &addrlen);
				if(clifd==-1) {
					fprintf(stderr, "accept failed: %s\n", strerror(errno));
					goto exit_failure;
				}
				fprintf(stderr, "Client connected: %s:%d\n", 
					inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
				if(fcntl(clifd, F_SETFD, fcntl(clifd, F_GETFD)|O_NONBLOCK)==-1) {
					fprintf(stderr, "fcntl failed: %s\n", strerror(errno));
					goto exit_failure;
				}
				int buflen = 65535;
				if(setsockopt(clifd, SOL_SOCKET, SO_SNDBUF, &buflen, sizeof buflen) < 0) {
					fprintf(stderr, "setsockopt failed: %s\n", strerror(errno));
					goto exit_failure;
				};
				open_connection_count++;
				// set open flag
				connections[clifd][0]=1;
				connections[clifd][1]=TYPE_CLI;
				connections[clifd][2]=SOCKS5_STATE_RCVBUF; // FLAG FOR INIT SOCKS5 negotiation
				connections[clifd][3]=0; // fd of target host
				connections[clifd][4]=time(NULL);
				connections[clifd][5]=0;

				// register in epoll 
				ev.events=EPOLLIN;
				ev.data.fd=clifd;
				if(epoll_ctl(epollfd,EPOLL_CTL_ADD, clifd, &ev)==-1) {
					fprintf(stderr, "epoll_ctl failed: %s\n", strerror(errno));
					goto exit_failure;
				}
				// continue loop wait for read events
			}
			// handle event from client/host
			else {
				handle_event(&events[i]);
			}
		}
	}
	pthread_join(tid, NULL);
	cleanup();
	exit(EXIT_SUCCESS);
exit_failure:
	cleanup();
	exit(EXIT_FAILURE);
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

void init_connections() 
{
	for(int i=0;i < MAX_OPEN; i++) 
		memset(&connections[i], 0, sizeof connections[i]);
}

void cleanup() 
{
	if(serverfd!=-1) close(serverfd);
	if(epollfd!=-1) close(epollfd);
	for(int i=0;i < MAX_OPEN; i++) if(connections[i][0]) close(i);
}

int sock_send(int fd, unsigned char*buf, size_t len)
{
	int ret=0;
	while(1) {
		ret = send(fd, buf, len, MSG_NOSIGNAL|MSG_DONTWAIT);
		if(ret < 0 && errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)
			continue;
		break;
	}
	return ret;
}

int sock_recv(int fd, unsigned char *buf, size_t len) 
{
	int ret=0;
	while(1) {
		ret = recv(fd, buf, len, MSG_NOSIGNAL|MSG_DONTWAIT);
		if(ret<0 && errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) 
			continue;
		break;
	}
	return ret;
}

void sigint_handle(int sig)
{
	(void) sig;
	interrupt=1; // useless flag xD
}

void handle_event(struct epoll_event *event)
{
	if(!connections[event->data.fd][0])
		return; // not an open connection
	// check for socket errors
	if( event->events&EPOLLERR || event->events&EPOLLRDHUP || event->events&EPOLLHUP) {
		fprintf(stderr, "handle_event: epoll event reporeted error fd=%d\n", event->data.fd);
		connection_close(event->data.fd);
		return;
	}
    // check wether socks5 cli or target host
    // check wether read/write
	socklen_t l = 0;
	int val=0;
	if(getsockopt(event->data.fd, SOL_SOCKET, SO_ERROR, &val, &l) < 0) {
		fprintf(stderr, "getsockopt failed: %s\n", strerror(errno));
        connection_close(event->data.fd);
        return;
    }
    if(val > 0)  { 
        fprintf(stderr, "SO_ERROR flag is set...\n");
        connection_close(event->data.fd);
        return ;
    }

	char buf[256] = {0};
	int len;
	if(connections[event->data.fd][2]==SOCKS5_STATE_RCVBUF && event->events&EPOLLIN) {
		// set busy flag
		__sync_lock_test_and_set(&connections[event->data.fd][5], 1);
		len=sock_recv(event->data.fd, buf, sizeof buf) ;
		if(len<0) {
			fprintf(stderr, "sock_recv failed: %s\n", strerror(errno));
			connection_close(event->data.fd);
			return;
		} 
		fprintf(stdout, "Received data len: %d bytes.\n", len);
		connections[event->data.fd][2]=SOCKS5_STATE_SNDBUF;
		connections[event->data.fd][4]= time(NULL);
		len = sock_send(event->data.fd, buf, strlen(buf));
        if(len < 0) {
            fprintf(stderr, "sock_send failed: %s\n", strerror(errno));
            connection_close(event->data.fd);
            return;
        }
        fprintf(stdout, "Sent data len: %d bytes.\n", len);
        connections[event->data.fd][2]=SOCKS5_STATE_RCVBUF;
		// rm busy flag
		__sync_lock_test_and_set(&connections[event->data.fd][5], 0);
		return ;
	}
	/*else if(connections[event->data.fd][2]==SOCKS5_STATE_SNDBUF && event->events&EPOLLOUT) {
		// set busy flag
		__sync_lock_test_and_set(&connections[event->data.fd][5], 1);
		strcpy(buf, "GOT YA!");
		/*socklen_t len=sizeof(int);
		int val=0;
		if(getsockopt(event->data.fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
			fprintf(stderr, "getsockopt failed: %s\n", strerror(errno));
			connection_close(event->data.fd);
			return;
		}
		if(val > 0)  { 
			fprintf(stderr, "SO_ERROR flag is set...\n");
			connection_close(event->data.fd);
			return ;
		}
		len = sock_send(event->data.fd, buf, strlen(buf));
		if(len < 0) {
			fprintf(stderr, "sock_send failed: %s\n", strerror(errno));
            connection_close(event->data.fd);
			return;
		} 
		fprintf(stdout, "Sent data len: %d bytes.\n", len);
		connections[event->data.fd][2]=SOCKS5_STATE_RCVBUF;
		// rm busy flag
		__sync_lock_test_and_set(&connections[event->data.fd][5], 0);
	}*/
}

void connection_close(int fd) {
	__sync_lock_test_and_set(&connections[fd][0], 0);
    __sync_lock_test_and_set(&connections[fd][1], 0);
    __sync_lock_test_and_set(&connections[fd][2], 0);
    __sync_lock_test_and_set(&connections[fd][3], 0);
	__sync_lock_test_and_set(&connections[fd][4], 0);
	__sync_lock_test_and_set(&connections[fd][5], 0);
	close(fd);
	__sync_fetch_and_sub(&open_connection_count, 1);
}

void *timeout_proc(void *args) {
	for(;;) {
		if(interrupt) 
			break;
		sleep(1);
		for(int i=0;i < MAX_OPEN; i++) {
			// currently just handle open fds
			if(connections[i][0]) {
				// check if busy
				if(__sync_fetch_and_add(&connections[i][5], 0)) 
					continue;
				if(time(NULL) - connections[i][4] >= server_timeout)
				{
					fprintf(stderr, "Closing connection...fd=%d\n", i);
					connection_close(i);
				}
			}
		}
	}
	pthread_exit(0);
}

