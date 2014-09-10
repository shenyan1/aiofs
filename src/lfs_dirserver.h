#ifndef __DIRSERVER_H
#define __DIRSERVER_H
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
#define MAX_CON (1200)

int response_client (int clifd, int shmid);
void *lfs_dirworker_thread_fn (void *arg);
#endif
