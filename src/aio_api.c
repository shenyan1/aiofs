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
#include "lfs_ops.h"
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

short
id2no (unsigned int id)
{
    short i;
    short len = sizeof (id_list) / sizeof (int *);
    for (i = 0; i < len; i++)
      {
	  if (id == id_list[i])
	      return i;
      }
    printf ("dingdong::UNKNOWN disk id--->%d\n", id);
    return len;
}


/*return value:
 *-1: failed
 *  0: successful
 *  1: successful and another round
 */
static int
trace_nextrequest (struct trace_entry *req)
{

    int ret = 0,fid;
    CQ_ITEM *item;
    item = cq_pop(RFS_CQ);
    if(item == NULL){
//	printf("queue is empty\n");
	return -1;
    }
    fid = item->fid;
    assert(fid>=0);
    if(item->fops == READ_COMMAND){
    	req->startbyte = item->offset+lfs_n.f_table[fid].meta_table[0];
    	req->bytecount = AIO_BLKSIZE;
    	req->rwType = 'R';
    }
    else{
	req->rwType = 'W';

    }
    if (req->bytecount < 0)
      {
	  printf ("mmmmmmmm\n");
	  return -1;
      }
    req->item = item;
    return ret;
}


int
finalize ()
{

    return 1;
}


void *
aio_completion_handler (void *thread_data)
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
	  //      printf("\n%d io_request completed\n\n", num);

	  for (i = 0; i < num; i++)
	    {
		this_io = (struct io_queue *) events[i].data;

		if (events[i].res2 != 0)
		  {
		      printf ("aio write error \n");
		  }
		if (events[i].obj != &this_io->iocb)
		  {
		      printf ("iocb is lost \n");
		      exit (1);
		  }
		if (events[i].res != this_io->iocb.u.c.nbytes)
		  {
		      printf ("rw missed bytes expect % ld,got % ld res2=%d \n",
			      this_io->iocb.u.c.nbytes, events[i].res,events[i].res2);
		     // exit (1);
		  }
		obj=this_io->item->obj;
		arc_read_done (obj);
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
inline void prep_aio(struct iocb *this_iocb,const struct trace_entry *request){
	struct object *obj = getobj(request->item->obj);
	  if (request->rwType == 'R')
	      io_prep_pread (this_iocb, lfs_n.fd, obj->obj_data->data,
			     request->bytecount, request->startbyte);
	  else
	      io_prep_pwrite (this_iocb, lfs_n.fd, obj->obj_data->data,
			      request->bytecount, request->startbyte);

}

void*
aio_start (void *p)
{
    struct trace_entry request;
    int ret;
    int i;
    int queue_idx = 0;
    struct io_queue *this_io;
    struct iocb *this_iocb;
    memset (&ioctx, 0, sizeof (ioctx));
    io_setup (AIO_MAXIO, &ioctx);

    while(1)
      {
	  // read one trace entry and initialize an aiocb
	  ret = trace_nextrequest (&request);
	  if(ret == -1)
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
		printf ("sleep");
		usleep (200);
		goto repeat;
	    }

	  if (AIO_BLKSIZE < request.bytecount)
	    {
		printf
		    ("%d buffer is allocated for a %d request, buffer is too small\n",
		     AIO_BLKSIZE, request.bytecount);
		exit (0);
	    }
/* 
 * make a iocb.
 */
	  assert(request.startbyte % 512==0);
	  //printf ("startbyte = %" PRIu64 "", request.startbyte);
	  this_iocb = &this_io->iocb;
          prep_aio(this_iocb,&request);
	  this_iocb->data = this_io;

	  ret = io_submit (ioctx, 1, &this_iocb);
#if 0
	  if (ret < 0)
	      printf ("io_submit io error\n");
	  else
	      printf ("\nsubmit  %d  read request\n", ret);
#endif
	  if (should_stop)
	      break;
      }
}

static void
sigint_handler (int f)
{
    printf ("get interrput\n");
    assert (0);
}
int aio_destroy(){


/*    while (should_stop == 0)
      {
	  printf ("wait\n");;
      }
*/
    free(IOCBQUEUE);   
    pthread_mutex_destroy (&IOCBQ_MUTEX);
    return 0;
}
void
aio_init ()
{

    int i;
    /* init aio_receiver thread.
     */
    if (pthread_create
	(&lfs_n.rfs_receiver_th, NULL, aio_completion_handler, (void *) NULL) != 0)
      {
	  printf ("Create completion thread error!\n");
	  exit (1);
      }

    signal (SIGINT, sigint_handler);
    pthread_mutex_init (&IOCBQ_MUTEX, NULL);
    cq_init ();
    IOCBQUEUE = (struct io_queue *) malloc (sizeof (struct io_queue) * QUEUE_SIZE);
    if (IOCBQUEUE == NULL)
      {
	  printf ("ioq out of memeory\n");
	  exit (1);
      }
 
/* init ioqueue.
 */
    for (i = 0; i < QUEUE_SIZE; i++)
	  memset (&IOCBQUEUE[i], 0, sizeof (struct io_queue));

    if (pthread_create
	(&lfs_n.rfs_worker_th, NULL, aio_start, (void *) NULL) != 0)
      {
	  printf ("Create create worker thread error!\n");
	  exit (1);
      }
     printf("finished\n");
}

