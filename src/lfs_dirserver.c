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
#include "lfs_sys.h"
#include "lfs_fops.h"
#include "rfs_req.h"
#include <sys/stat.h>
#include "lfs_dirserver.h"
#define MAX_CON (1200)
#define PROTOCAL_SIZE (1+4+4+8+4)

extern lfs_info_t lfs_n;
static struct epoll_event *events;

int getplen (char *buf)
{
    char op;
    int len = *(buf + 1);
    op = *buf;
    if (op == CLOSE_COMMAND)
      {

	  return 1 + sizeof (int);
      }
    return len + 2;
}

void *lfs_dirworker_thread_fn (void *arg)
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


    listen_fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
      {
	  perror ("cannot create listening socket");
      }
    else
      {
	  srv_addr.sun_family = AF_UNIX;
	  strncpy (srv_addr.sun_path, DIR_UNIX_DOMAIN,
		   sizeof (srv_addr.sun_path) - 1);
	  unlink (DIR_UNIX_DOMAIN);
	  ret =
	      bind (listen_fd, (struct sockaddr *) &srv_addr,
		    sizeof (srv_addr));
	  if (ret == -1)
	    {
		perror ("cannot bind server socket");
		lfs_printf ("srv_addr:%p", &srv_addr);
		close (listen_fd);
		unlink (DIR_UNIX_DOMAIN);
		exit (1);
	    }
      }

    ret = listen (listen_fd, 1);
    if (ret == -1)
      {
	  perror ("cannot listen the client connect request");
	  close (listen_fd);
	  unlink (DIR_UNIX_DOMAIN);
	  exit (1);
      }
    chmod (DIR_UNIX_DOMAIN, 00777);	//设置通信文件权限


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
			   accept (listen_fd,
				   (struct sockaddr *) &clientaddr,
				   (socklen_t *) & addrlen)) == -1)
			{
			    perror ("Server-accept() error lol!");
			}
		      else
			{
			    //      lfs_printf("Server-accept() is OK...\n");
			    ev.events = EPOLLIN;
			    ev.data.fd = newfd;
			    if (epoll_ctl
				(epfd, EPOLL_CTL_ADD, newfd, &ev) < 0)
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
			    //  lfs_printf ("find event");
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
				 recv (client_fd, buf, 1024, 0)) <= 0)
			      {
				  if (nbytes == 0)
				    {
				    }
				  else
				    {
					lfs_printf ("recv() error lol! %d",
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
				  int tlen;
				  //  lfs_printf ("nbytes=%d,recv %s,%c", nbytes, buf,
				  //        buf[6]);
				  process_dirrequest (buf, client_fd);
				  tlen = getplen (buf);
				  memset (buf, 0, tlen);

			      }
			    break;
			}
		  }
	    }
      }
    return 0;
}
