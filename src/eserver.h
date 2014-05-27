#ifndef __ESERVER_H
#define __ESERVER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include "lfs.h"
#define UNIX_DOMAIN "/tmp/UNIX2.domain1"
#define MAX_CON (1200)


void *
lfs_dispatcher_thread_fn (void *arg);
#endif
