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
#include "lfs_thread.h"

#define BLOCK_SIZE	512
#define CHUNK_SIZE  128	//64KB
#define REFERENCE_DISKS	1
//#define SEC_INTERVAL	3600
#define MAXSIZE 10000

uint64_t offs[MAXSIZE];

int	raid_fd;
//long		period_nr=0;
struct trace_entry{
	short	 	devno;
	uint64_t 	startbyte;
	int	 		bytecount;
	char 		rwType;
};

//struct aiocb aiocb_array[65536];
long	record_count=0;
pthread_mutex_t	iocb_queue_mutex;

int	should_stop =0;

struct timeval	test_start;
io_context_t ioctx;
#define	AIO_BLKSIZE	LFS_BLKSIZE
#define	AIO_MAXIO	 	512
#define	QUEUE_SIZE		4*AIO_MAXIO

struct io_queue{
	struct iocb 		iocb;
	char				*buf;
	int	 			ref_cnt;
};

struct	io_queue *ioq;



unsigned int id_list[]=
{
0,
1
};
short id2no(unsigned int id){
	    short i;
	    short len=sizeof(id_list)/sizeof(int*);
	    for (i=0;i<len;i++){
	        if (id==id_list[i])
	            return i;
	    }
	    printf("dingdong::UNKNOWN disk id--->%d\n",id);
	    return len;
}


/*return value:
 *-1: failed
 *  0: successful
 *  1: successful and another round
 */
int trace_nextrequest(struct trace_entry* req,int idx)
{
	
	int	ret = 0;
	req->startbyte = offs[idx];
	req->bytecount = AIO_BLKSIZE;
  	req->rwType='r';

	
	if (req->bytecount <0){
	  	printf("mmmmmmmm\n");
  		return -1;
	}
	  	if (req->rwType=='r'){
  		req->rwType='R';
  	}else if(req->rwType=='w'){
  		req->rwType='W';
  	}  	
  	 
	return ret;
}

 
int finalize()
{
	
	pthread_mutex_destroy(&iocb_queue_mutex);
	return 1;
}


void* aio_completion_handler( void * thread_data )
{
	struct io_event events[AIO_MAXIO];
	struct io_queue	*this_io;
	int		num,i;
	int total=0;
	while(1){
		num = io_getevents(ioctx, 1, AIO_MAXIO, events, NULL);
		if(should_stop)
			break;
	//	printf("\n%d io_request completed\n\n", num);

		for(i=0;i<num;i++){
			this_io = (struct io_queue*)events[i].data;

			if (events[i].res2 != 0) {
				printf("aio write error \n");
			}
			if (events[i].obj != &this_io->iocb) {
				printf("iocb is lost \n");
				exit(1);
			}
//			printf("get read=%p",&this_io->iocb);
			if (events[i].res != this_io->iocb.u.c.nbytes) {
				printf("rw missed bytes expect % ld got % ld \n", this_io->iocb.u.c.nbytes, events[i].res);
				exit(1);
			}

			record_count++;
			if(record_count==65536){
				record_count=0;
			}
		}

		pthread_mutex_lock (&iocb_queue_mutex);
		for(i=0;i<num;i++){
			this_io = (struct io_queue*)events[i].data;
			this_io->ref_cnt =0;
		}
		pthread_mutex_unlock (&iocb_queue_mutex);
		total+= num;
		if(total==MAXSIZE)
			break;
	}
	should_stop=1;
	pthread_exit(NULL) ;
}


int req_count=0;
void io_play()
{
	struct trace_entry request;
	int			ret;	
	int			i;
	int			queue_idx =0;
	struct io_queue		*this_io;
	struct iocb			*this_iocb;
	memset(&ioctx, 0, sizeof(ioctx));
	io_setup(AIO_MAXIO, &ioctx);

	ioq =  (struct io_queue*)malloc(sizeof(struct io_queue)*QUEUE_SIZE);
	if(ioq==NULL){
		printf( "ioq out of memeory\n");
		exit(1);
	 }
	
	for(i=0; i<QUEUE_SIZE; i++){
		memset(&ioq[i],0,sizeof(struct io_queue));
		ret = posix_memalign((void **)&	ioq[i].buf, 512, AIO_BLKSIZE);
		if(ret < 0) {
			printf("posix_memalign error, line %d\n", __LINE__);
			exit(1);
		}
	}

	//create the aio_completion_handler thread
//	if(pthread_detach(completion_th) !=0){
	//	printf ("Detach completion thread error!\n");
	//	exit (1);
//	}	
	int idx=0;
	for(idx=0;idx<MAXSIZE;idx++){
		// read one trace entry and initialize an aiocb
		ret = trace_nextrequest(&request,idx);
		
repeat:
		//allocate a free ioqueue
		pthread_mutex_lock (&iocb_queue_mutex);
		for(i=0; i<QUEUE_SIZE; i++){
			this_io = &ioq[(queue_idx+i)%QUEUE_SIZE];
			if(!this_io->ref_cnt){
				queue_idx+=i+1;
				this_io->ref_cnt =1;
				break;
			}
		}
		pthread_mutex_unlock (&iocb_queue_mutex);
		if(i == QUEUE_SIZE){
			printf("sleep");
			usleep(200);
			goto repeat;
		}

		if (AIO_BLKSIZE < request.bytecount){
			printf("%d buffer is allocated for a %d request, buffer is too small\n", AIO_BLKSIZE, request.bytecount);
			exit(0);
		} 			

		request.startbyte -= request.startbyte % 512;
		printf("startbyte = %"PRIu64"",request.startbyte);
		this_iocb = &this_io->iocb;
		if(request.rwType == 'R' )
			io_prep_pread (this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		else
			io_prep_pwrite(this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		this_iocb->data = this_io;
		printf("submit iocb=%p\n",this_iocb);
		//sleep some time if necessary

		//play the request
		ret = io_submit(ioctx, 1, &this_iocb);
#if 0
		if(ret < 0)
			printf("io_submit io error\n");
		else
			printf("\nsubmit  %d  read request\n", ret);
#endif		
		req_count++;
		if (should_stop)
			break;

	}
	while(should_stop==0){
		printf("wait\n");	;
	}
	free(ioq);
}

static void sigint_handler(int f){
	printf("get interrput\n");
	assert(0);
}

int aio_init(){

	pthread_t receiver_th;
	/* init aio_receiver thread.
         */
	if(pthread_create(&receiver_th, NULL, aio_completion_handler, (void*)NULL) !=0){
		printf ("Create completion thread error!\n");
		exit(1); 
	}

	signal(SIGINT,sigint_handler);
	pthread_mutex_init (&iocb_queue_mutex,NULL);
	

}
int main(int argc, char * argv[])
{
	int flags;

	srand(0);
	
	flags =   O_RDWR|  O_LARGEFILE;
	raid_fd =open(argv[1],flags,0777);
	if (raid_fd < 0) {
		printf("RAID5 open failure:%d\n",raid_fd);
		exit(1) ;
	}
	io_play();

	finalize();
	close(raid_fd);

	return 0;
}




