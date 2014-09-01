#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_SEND 1025
#define UNIX_PATH "/tmp/UNIX2.domain1"
//#define UNIX_PATH "/tmp/sinfor"
char *
rfs_read (int id, char *buffer, uint64_t size, uint64_t offset)
{
    int shmid, connfd;
    char *pro, *buf, *ptr;
//  ops = READ_COMMAND;
    pro = malloc (READ_SIZE);
    memset(pro,0,READ_SIZE);
    if (!buffer)
      {
	  printf ("error malloc");
	  exit (1);
      }
    ptr = pro;
    *ptr = (char)READ_COMMAND;
    pro += 1;
    *(int *) pro = id;
//  memcpy(pro,&id,sizeof(id));
    pro += sizeof (int);
/*skip the null shmid*/
    pro += sizeof (int);
    *(uint64_t *) pro = offset;
    pro += sizeof (offset);

    *(uint64_t *) pro = size;
    printf("op=%d,id=%d",READ_COMMAND,id);
    return ptr;  
}  


void
dump_unix (int sock_fd)
{
    char tmp[MAX_SEND] = { 0 };
    char recv[MAX_SEND] = { 0 };
    while (fgets (tmp, MAX_SEND, stdin) != NULL)
      {
	  write (sock_fd, tmp, strlen (tmp));
	  read (sock_fd, recv, MAX_SEND);
	  printf ("data : %s\n", recv);
	  bzero (tmp, MAX_SEND);
	  bzero (recv, MAX_SEND);
      }
}

int
main (int argc, char **argv)
{
    int conn_sock = socket (AF_LOCAL, SOCK_STREAM, 0);
    if (conn_sock == -1)
      {
	  perror ("socket fail ");
	  return -1;
      }
    struct sockaddr_un addr;
    bzero (&addr, sizeof (addr));
    addr.sun_family = AF_LOCAL;
    strcpy ((void *) &addr.sun_path, UNIX_PATH);
    if (connect (conn_sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
      {
	  perror ("connect fail ");
	  return -1;
      }
    dump_unix (conn_sock);
    close (conn_sock);
    return 0;
}
