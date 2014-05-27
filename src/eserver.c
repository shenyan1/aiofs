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
#include <sys/stat.h>
#define UNIX_DOMAIN "/tmp/UNIX2.domain1"
#define MAX_CON (1200)
#define PROTOCAL_SIZE (1+4+4+8+4)
/*
 * FOPS:1B Inodeid:4B shmid:4B,offset:8B,size:4B
 */
extern lfs_info_t lfs_n;
static struct epoll_event *events;

static inline int
ioreq_enqueue (char *socket_buf)
{

    CQ_ITEM *item = cqi_new();
    sprintf(socket_buf,"%c%d%d%"PRIu64"%d"
	,item->fops,item->fid,item->shmid,item->offset,item->size);
    item->fops = item->fops-'0';
//    printf("readstate =%d",_obj->read_state);
    cq_push(lfs_n.req_queue,item);

    return 1;
}

void *
lfs_dispatcher_thread_fn (void *arg)
{
    struct sockaddr_in clientaddr;
    int fdmax;
    int newfd;
    char buf[1024];
    int nbytes;
    int addrlen;
    int ret;
    int epfd = -1;
    int res = -1;
    struct epoll_event ev;
    int index = 0;
    int listen_fd, client_fd = -1;
    struct sockaddr_un srv_addr;

    //创建用于通信的套接字，通信域为UNIX通信域

    listen_fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
      {
	  perror ("cannot create listening socket");
      }
    else
      {
	  //设置服务器地址参数
	  srv_addr.sun_family = AF_UNIX;
	  strncpy (srv_addr.sun_path, UNIX_DOMAIN,
		   sizeof (srv_addr.sun_path) - 1);
	  unlink (UNIX_DOMAIN);
	  //绑定套接字与服务器地址信息
	  ret =
	      bind (listen_fd, (struct sockaddr *) &srv_addr,
		    sizeof (srv_addr));
	  if (ret == -1)
	    {
		perror ("cannot bind server socket");
		close (listen_fd);
		unlink (UNIX_DOMAIN);
		exit (1);
	    }
      }

    ret = listen (listen_fd, 1);
    if (ret == -1)
      {
	  perror ("cannot listen the client connect request");
	  close (listen_fd);
	  unlink (UNIX_DOMAIN);
	  exit (1);
      }
    chmod (UNIX_DOMAIN, 00777);	//设置通信文件权限


    fdmax = listen_fd;		/* so far, it's this one */

    events = calloc (MAX_CON, sizeof (struct epoll_event));
    if ((epfd = epoll_create (MAX_CON)) == -1)
      {
	  perror ("epoll_create");
	  exit (1);
      }
    ev.events = EPOLLIN;
    ev.data.fd = fdmax;
    if (epoll_ctl (epfd, EPOLL_CTL_ADD, fdmax, &ev) < 0)
      {
	  perror ("epoll_ctl");
	  exit (1);
      }
    //time(&start);
    for (;;)
      {
	  res = epoll_wait (epfd, events, MAX_CON, -1);
	  client_fd = events[index].data.fd;

	  for (index = 0; index < MAX_CON; index++)
	    {
		if (client_fd == listen_fd)
		  {
		      addrlen = sizeof (clientaddr);
		      if ((newfd =
			   accept (listen_fd, (struct sockaddr *) &clientaddr,
				   (socklen_t *)&addrlen)) == -1)
			{
			    perror ("Server-accept() error lol!");
			}
		      else
			{
			    //      printf("Server-accept() is OK...\n");
			    ev.events = EPOLLIN;
			    ev.data.fd = newfd;
			    if (epoll_ctl (epfd, EPOLL_CTL_ADD, newfd, &ev) <
				0)
			      {
				  perror ("epoll_ctl");
				  exit (1);
			      }
			}
		      break;
		  }
		else
		  {
		      if (events[index].events & EPOLLHUP)
			{
			    if (epoll_ctl
				(epfd, EPOLL_CTL_DEL, client_fd, &ev) < 0)
			      {
				  perror ("epoll_ctl");
			      }
			    close (client_fd);
			    break;
			}
		      if (events[index].events & EPOLLIN)
			{
/* going to recv data
 */
			    
			    if ((nbytes =
				 recv (client_fd, buf, sizeof (buf), 0)) <= 0)
			      {
				  if (nbytes == 0)
				    {
					//      printf("socket %d hung up\n", client_fd);
				    }
				  else
				    {
					printf ("recv() error lol! %d",
						client_fd);
					perror ("");
				    }

				  if (epoll_ctl
				      (epfd, EPOLL_CTL_DEL, client_fd,
				       &ev) < 0)
				    {
					perror ("epoll_ctl");
				    }
				  close (client_fd);
			      }
			    else
			      {
				  printf ("recv %s", buf);
				  if (send (client_fd, buf, nbytes, 0) == -1)
				      perror ("send() error lol!");
			      }
			    break;
			}
		  }
	    }
      }
    return 0;
}
