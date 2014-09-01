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
#include "lfs_fops.h"
#include "lfs_sys.h"
#include "rfs_req.h"
#include <sys/stat.h>
#define UNIX_DOMAIN "/tmp/UNIX2.domain1"
#define MAX_CON (1200)
#define PROTOCAL_SIZE (1+4+4+8+4)
/*
 * FOPS:1B Inodeid:4B shmid:4B,offset:8B,size:4B
 */
extern lfs_info_t lfs_n;
static struct epoll_event *events;
#if 0
static inline int ioreq_enqueue (char *socket_buf)
{

    CQ_ITEM *item = cqi_new ();
    sprintf (socket_buf, "%c%d%d%" PRIu64 "%d", item->fops, item->fid,
	     item->shmid, item->offset, item->size);
    item->fops = item->fops - '0';
//    lfs_printf("readstate =%d",_obj->read_state);
    cq_push (lfs_n.req_queue, item);

    return 1;
}
#endif


/* this method is used for N(blocks) <= 1.
 */
int response_client (int clifd, int value)
{
    char num[5];
    int *ptr;
    ptr = num;
    *ptr = value;
    if (clifd < 0)
	lfs_printf ("client socket is invalid\n");
    if (write (clifd, num, 5 * sizeof (char)) < 0)
      {
	  perror ("response to client failed with -1");
	  return -1;

      }
    return LFS_SUCCESS;
}

/* rfs_iowrite put the item into aio_req.
 */
int rfs_iowrite (CQ_ITEM * item)
{
    assert (item != NULL);
    assert (item->shmid > 0);
    cq_push (RFS_AIOQ, item);
    return true;
}

int rfs_ioread (CQ_ITEM * item)
{

    uint64_t l_off, r_off, offset, id, size;
    int nblks;
    int bufoff, tocpy;
    struct object *obj;
#ifndef USE_SARC
    struct __arc_object *entry;
#else
    struct __sarc_object *entry;
#endif
    offset = item->offset;
    size = item->size;
    id = item->fid;
    l_off = getdiskpos (offset);
    r_off = getdiskpos (offset + size);
    nblks = (getdiskrpos (offset + size) - getdiskpos (offset)) >> 20;
    bufoff = offset - l_off;
    tocpy = LFS_BLKSIZE - offset + bufoff;
    if (id < 0 || id > lfs_n.max_files)
	return -LFS_EPM;
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  lfs_printf ("read refused id=%" PRIu64 " isfree=%d\n ", id,
		      lfs_n.f_table[id].is_free);
	  return -LFS_EPM;
      }
    /* the block is across only 1 block.
     */
    if (nblks <= 0)
      {
	  /*return null,no I/O is valid. */
      }
    else if (nblks == 1)
      {
	  entry = __arc_lookup (lfs_n.arc_cache, item);
	  if (entry)
	    {

		obj = __arc_list_entry (entry, struct object, entry);
		response_client (item->clifd, obj->obj_data->shmid);
	    }
	  else
	    {
		/* the logical is finished in arc level. */
		return LFS_PENDING;
	    }
      }
    else if (nblks > 1)
      {
	  /*For read requests more than one block. */
      }
/*
    for (i = 0; i < nblks; i++)
      {
#ifndef USE_SARC
	  entry = __arc_lookup (lfs_n.arc_cache, id, l_off);
#else
	  entry = __sarc_lookup (lfs_n.arc_cache, id, l_off);
#endif


	  obj = __arc_list_entry (entry, struct object, entry);
	  bufoff = offset - obj->offset;
	  tocpy = (int) MIN (LFS_BLKSIZE - bufoff, size);
	  memcpy (buffer, obj->obj_data->data + bufoff, tocpy);
	  offset += tocpy;
	  size -= tocpy;
	  buffer = buffer + tocpy;
	  l_off += LFS_BLKSIZE;
      }
*/
    return 0;
}

/*
  rworker_thread_fn: get request from request queue and process the request.
 */
void *lfs_rworker_thread_fn (void *arg)
{

    int ret = 0;
    CQ_ITEM *item;
    while (1)
      {
	  item = cq_pop (RFS_RQ);
	  if (item == NULL)
	    {
		continue;
	    }
	  assert (item->fid >= 0);
	  if (item->fops == READ_COMMAND)
	    {
		rfs_ioread (item);
	    }
	  else if (item->fops == WRITE_COMMAND)
	    {
		rfs_iowrite (item);
	    }
	  else
	    {

	    }
      }
    return (void *) ret;
}


void *lfs_dispatcher_thread_fn (void *arg)
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
	  strncpy (srv_addr.sun_path, UNIX_DOMAIN,
		   sizeof (srv_addr.sun_path) - 1);
	  unlink (UNIX_DOMAIN);
	  ret =
	      bind (listen_fd, (struct sockaddr *) &srv_addr,
		    sizeof (srv_addr));
	  if (ret == -1)
	    {
		perror ("cannot bind server socket");
		lfs_printf ("srv_addr:%p", &srv_addr);
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

				  lfs_printf ("nbytes=%d,recv %s,%c", nbytes,
					      buf, buf[6]);
				  process_request (buf, client_fd);
				  memset (buf, 0, 4);

			      }
			    break;
			}
		  }
	    }
      }
    return 0;
}
