#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <errno.h>

int main() {
	int fd, epollfd;
	struct sockaddr_in addr;
	struct epoll_event ev, events[128];
	socklen_t len;
	fd=socket(AF_INET, SOCK_STREAM, 0);
	if(fd==-1) {
		perror("socket");
		return -1;
	}
	int flg = fcntl(fd, F_GETFD, 0);
    fcntl(fd, F_SETFD, flg | O_NONBLOCK);
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(1080);
	len=sizeof addr;
	if(connect(fd, (struct sockaddr *)&addr, len) < 0) {
		perror("connect");
		return -1;
	}
	ev.events=EPOLLIN|EPOLLOUT;
	ev.data.fd=fd;
	epollfd = epoll_create1(0);
	if(epollfd < 0) {
		perror("epoll_create1");
		return -1;
	}
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev)<0) {
		perror("epoll_ctl");
		return -1;
	}
	char buf[2];
	if(write(fd, "hi", 2) < 0) perror("write");
	if(read(fd, buf, 2) < 0) perror("read");
	for(;;) {
		int nfds=epoll_wait(epollfd, events, sizeof events/sizeof events[0], -1);
		for(int i=0; i < nfds; i++) {
			if(events[i].data.fd==fd) {
				if(events[i].events&EPOLLIN) {
					char buf[256];
					int len;
					int total=0;
repeat_1:
					while((len=read(fd, buf, sizeof buf)) == sizeof buf) {
						total+=len;
					}
					if(len < 0 && errno==EAGAIN || errno==EWOULDBLOCK)
						goto repeat_1;
					printf("Received %d bytes\n", total);
				}
				else if(events[i].events&EPOLLOUT) {
					char buf[256];
                    int len;
                    int total=0;
					strcpy(buf, "hello, world C is rocks for sysprogramming!\n");
repeat_2:
                    len=write(fd, buf, strlen(buf));
                    if(len < 0 && errno==EAGAIN || errno==EWOULDBLOCK)
                        goto repeat_2;
                    printf("Wrote %d bytes\n", len);
				}
			}
		}
	}
	return 0;
}
