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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <getopt.h>
#include <signal.h>

extern char *optarg;
extern int optind, opterr, optopt;


void usage();
void version();
void daemonize();

int main(int argc,char *argv[])
{
  struct option long_opts[] = {{"help", no_argument, NULL, 0},
  {"version", no_argument, NULL, 0},
  {"daemon", no_argument, NULL, 0},
  {"debug", no_argument, NULL, 0},
  {"port", required_argument, NULL, 0},
  {"address", required_argument, NULL, 0}};
  int opt;
  int daemon = 0;
  int debug  = 0;
  while((opt=getopt_long(argc,argv, "hvDdp:a:", long_opts, &optind))!=-1) {
    switch(opt) {
      case 'h': 
        usage(); 
        exit(EXIT_SUCCESS);
      case 'v':
        version(); 
        break;
      case 'D':
        daemon = 1;
        break;
      case 'd': 
        debug = 1;
        break;
      case 'a': 
        printf("address: %s\n", optarg); 
        break;
      case 'p': 
        printf("port: %s\n", optarg); 
        break;
      default:
        usage();
        exit(EXIT_FAILURE);
    }
  }
  if(daemon) daemonize();
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
  fprintf(stderr, "Copyright (C) 2024 lskibu\n\n");
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
  chdir("/");
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
