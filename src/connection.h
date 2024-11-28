#ifndef CONNECTION_H
#define CONNECTION_H

#include <time.h>
#include "socks5.h"

enum CONNECTION_TYPE {
	CCLIENT,
	CTARGET
};


struct connection {  
	int fd;
	time_t last_recv;
	int state;
	int tfd;
	unsigned char lock;
};



#endif
