#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <libaio.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <features.h>
#include <sys/vfs.h>
#include "lfs.h"
#include "lfs_sys.h"
#include "lfs_ops.h"
#include "rfs_req.h"
#include "aio_api.h"
#include "lfs_thread.h"
#define BLOCK_SIZE	512
#define CHUNK_SIZE  128		//64KB
#define REFERENCE_DISKS	1
//#define SEC_INTERVAL  3600
#define MAXSIZE 10000

uint64_t offs[MAXSIZE];
extern lfs_info_t lfs_n;
int raid_fd;
//long          period_nr=0;
//struct aiocb aiocb_array[65536];

int should_stop = 0;

io_context_t ioctx;
#define	AIO_BLKSIZE	LFS_BLKSIZE
#define	AIO_MAXIO	 	512
#define	QUEUE_SIZE		4*AIO_MAXIO


struct io_queue *ioq;



unsigned int id_list[] = {
    0,
    1
};

short id2no (unsigned int id)
{
    short i;
    short len = sizeof (id_list) / sizeof (int *);
    for (i = 0; i < len; i++)
      {
	  if (id == id_list[i])
	      return i;
      }
    lfs_printf ("dingdong::UNKNOWN disk id--->%d\n", id);
    return len;
}


/*return value:
 *-1: failed
 *  0: successful
 *  1: successful and another round
 */
static int trace_nextrequest (struct trace_entry *req)
{

    int ret = 0, fid;
    u64 blkptr;
    CQ_ITEM *item;
    item = cq_pop (RFS_AIOQ);
    if (item == NULL)
      {
//      lfs_printf("queue is empty\n");
	  return -1;
      }
    fid = item->fid;
    assert (fid >= 0);
    if (item->fops == READ_COMMAND)
      {
	  blkptr = lfsgetblk (&lfs_n, fid, item->offset);
	  if (blkptr != 0)
	    {
		req->startbyte = blkptr;;
		req->bytecount = AIO_BLKSIZE;
		req->rwType = 'R';
	    }
      }
    else
      {
	  assert (item->fops == WRITE_COMMAND);

	  blkptr = lfsgetblk (&lfs_n, fid, item->offset);
	  if (blkptr != 0)
	    {
		req->startbyte = blkptr;;
		req->bytecount = item->size;
		req->rwType = 'W';
	    }
	  else
	    {
		response_client (item->clifd, -1);
	    }
	  lfs_printf ("req->startbyte =%" PRIu64 ",offset=%" PRIu64
		      ",meta_table[0]=%" PRIu64 "\n", req->startbyte,
		      item->offset, lfs_n.f_table[fid].meta_table[0]);
//        lfs_printf ("req's data=%c%c\n", *getshmptr (item->shmid),
//                    *(getshmptr (item->shmid) + 1));
      }
    if (req->bytecount < 0)
      {
	  lfs_printf ("mmmmmmmm\n");
	  return -1;
      }
    req->item = item;
    return ret;
}


int finalize ()
{

    return 1;
}


void *aio_completion_handler (void *thread_data)
{
    struct io_event events[AIO_MAXIO];
    struct io_queue *this_io;
    int num, i;
    struct __arc_object *obj;
    while (1)
      {
	  num = io_getevents (ioctx, 1, AIO_MAXIO, events, NULL);
	  if (should_stop)
	      break;
	  //      lfs_printf("\n%d io_request completed\n\n", num);

	  for (i = 0; i < num; i++)
	    {
		this_io = (struct io_queue *) events[i].data;

		if (events[i].res2 != 0)
		  {
		      lfs_printf ("aio write error \n");
		  }
		if (events[i].obj != &this_io->iocb)
		  {
		      lfs_printf ("iocb is lost \n");
		      exit (1);
		  }
		if (events[i].res != this_io->iocb.u.c.nbytes)
		  {
		      lfs_printf
			  ("rw missed bytes expect % ld,got % ld res2=%d \n",
			   this_io->iocb.u.c.nbytes, events[i].res,
			   events[i].res2);
		      // exit (1);
		  }
		if (this_io->item->fops == READ_COMMAND)
		  {
		      obj = this_io->item->obj;
		      arc_read_done (obj);
		      printf ("whats up");
		  }
		else if (this_io->item->fops == WRITE_COMMAND)
		  {

		      write_done (this_io->item, LFS_SUCCESS);
		  }
	    }

	  pthread_mutex_lock (&IOCBQ_MUTEX);

	  for (i = 0; i < num; i++)
	    {
		this_io = (struct io_queue *) events[i].data;
		this_io->ref_cnt = 0;
	    }
	  pthread_mutex_unlock (&IOCBQ_MUTEX);

      }

    should_stop = 1;
    pthread_exit (NULL);
}

inline void
prep_aio (struct iocb *this_iocb, const struct trace_entry *request)
{
    struct object *obj = getobj (request->item->obj);

    char *ptr;
    if (request->rwType == 'R')
	io_prep_pread (this_iocb, lfs_n.fd, obj->obj_data->data,
		       request->bytecount, request->startbyte);
    else if (request->rwType == 'W')
      {
	  ptr = request->item->_ptr = getshmptr (request->item->shmid);
	  if (ptr == NULL)
	    {
		lfs_printf ("pre_aio failed");
		assert (0);
		return;
	    }

	  lfs_printf ("prep_aio write\n");
	  io_prep_pwrite (this_iocb, lfs_n.fd, ptr,
			  request->bytecount, request->startbyte);
      }
}

void *aio_start (void *p)
{
    struct trace_entry request;
    int ret;
    int i;
    int queue_idx = 0;
    struct io_queue *this_io;
    struct iocb *this_iocb;
    memset (&ioctx, 0, sizeof (ioctx));
    io_setup (AIO_MAXIO, &ioctx);

    while (1)
      {
	  // read one trace entry and initialize an aiocb
	  ret = trace_nextrequest (&request);
	  if (ret == -1)
	      continue;
	repeat:
	  //allocate a free ioqueue
	  pthread_mutex_lock (&IOCBQ_MUTEX);
	  for (i = 0; i < QUEUE_SIZE; i++)
	    {
		this_io = &IOCBQUEUE[(queue_idx + i) % QUEUE_SIZE];
		if (!this_io->ref_cnt)
		  {
		      queue_idx += i + 1;
		      this_io->ref_cnt = 1;
		      this_io->item = request.item;
		      break;
		  }
	    }
	  pthread_mutex_unlock (&IOCBQ_MUTEX);
	  if (i == QUEUE_SIZE)
	    {
		//      lfs_printf ("sleep");
		usleep (200);
		goto repeat;
	    }

	  if (AIO_BLKSIZE < request.bytecount)
	    {
		lfs_printf
		    ("%d buffer is allocated for a %d request, buffer is too small\n",
		     AIO_BLKSIZE, request.bytecount);
		exit (0);
	    }
/* 
 * make a iocb.
 */
	  assert (request.startbyte % 512 == 0);
	  //lfs_printf ("startbyte = %" PRIu64 "", request.startbyte);
	  this_iocb = &this_io->iocb;
	  prep_aio (this_iocb, &request);
	  this_iocb->data = this_io;

	  ret = io_submit (ioctx, 1, &this_iocb);
	  if (ret < 0)
	      lfs_printf ("io_submit io error\n");

	  if (should_stop)
	      break;
      }
    return NULL;
}

static void sigint_handler (int f)
{
    lfs_printf ("get interrput\n");
    cv_broadcast (&lfs_n.stop_cv);
//    assert(0);
}

int aio_destroy ()
{
/*    while (should_stop == 0)
      {
	  lfs_printf ("wait\n");;
      }
*/
    free (IOCBQUEUE);
    pthread_mutex_destroy (&IOCBQ_MUTEX);
    return 0;
}

void aio_init ()
{

    int i;
    /* init aio_receiver thread.
     */
    if (pthread_create
	(&lfs_n.rfs_receiver_th, NULL, aio_completion_handler,
	 (void *) NULL) != 0)
      {
	  lfs_printf ("Create completion thread error!\n");
	  exit (1);
      }

    signal (SIGINT, sigint_handler);
    pthread_mutex_init (&IOCBQ_MUTEX, NULL);

    aioq_init ();
    IOCBQUEUE =
	(struct io_queue *) malloc (sizeof (struct io_queue) * QUEUE_SIZE);
    if (IOCBQUEUE == NULL)
      {
	  lfs_printf ("ioq out of memeory\n");
	  exit (1);
      }

/* init ioqueue.
 */
    for (i = 0; i < QUEUE_SIZE; i++)
	memset (&IOCBQUEUE[i], 0, sizeof (struct io_queue));

    if (pthread_create
	(&lfs_n.rfs_worker_th, NULL, aio_start, (void *) NULL) != 0)
      {
	  lfs_printf ("Create create worker thread error!\n");
	  exit (1);
      }
    //   lfs_printf ("finished\n");
}
