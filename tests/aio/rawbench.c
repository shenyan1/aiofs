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


#define BLOCK_SIZE	512
#define CHUNK_SIZE  128	//64KB
#define REFERENCE_DISKS	1
//#define SEC_INTERVAL	3600
#define MAXSIZE 10000

int offs[MAXSIZE];

FILE *tracefile;
int	raid_fd;
FILE *compfile;
FILE *respfile;
//long		period_nr=0;
struct trace_entry{
	short	 	devno;
	long long 	startbyte;
	int	 		bytecount;
	char 		rwType;
	double		reqtime;	
};

//struct aiocb aiocb_array[65536];
double		*complete_time;//65536 is chosen according to /proc/sys/fs/aio_max_nr 
long			*response_time;
long	record_count=0;
pthread_mutex_t	mutex;

int	should_stop =0;

struct timeval	test_start;
io_context_t ioctx;
#define	AIO_BLKSIZE	512*256
#define	AIO_MAXIO	 	512
#define	QUEUE_SIZE		4*AIO_MAXIO

struct io_queue{
	struct iocb 		iocb;
	char				*buf;
	double			issue_time;	
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
uint64_t cur_usec(void)
{
    struct timeval __time;
    unsigned long long cur_usec;

    gettimeofday(&__time, NULL);
    cur_usec =  __time.tv_sec;
    cur_usec *= 1000000;
    cur_usec += __time.tv_usec;

    return cur_usec;
}


int initialize()
{
	pthread_mutex_init (&mutex,NULL);
	return 1;
}

/*return value:
 *-1: failed
 *  0: successful
 *  1: successful and another round
 */
int trace_nextrequest(struct trace_entry* req,int idx)
{
	
	unsigned int u_devno;
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
	
	pthread_mutex_destroy(&mutex);
	return 1;
}

double elapse_sec()
{
	struct timeval current_tv;
	
	gettimeofday(&current_tv,NULL);
	return (double)(current_tv.tv_sec-test_start.tv_sec)+(double)(current_tv.tv_usec-test_start.tv_usec)/1000000.0;
}

void* aio_completion_handler( void * thread_data )
{
	struct io_event events[AIO_MAXIO];
	struct io_queue	*this_io;
	int		num,i,j;
	double comp_time,resp_time;
	int total=0;
	while(1){
		num = io_getevents(ioctx, 1, AIO_MAXIO, events, NULL);
		if(should_stop)
			break;
	//	printf("\n%d io_request completed\n\n", num);

		comp_time =elapse_sec();
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

		pthread_mutex_lock (&mutex);
		for(i=0;i<num;i++){
			this_io = (struct io_queue*)events[i].data;
			this_io->ref_cnt =0;
		}
		pthread_mutex_unlock (&mutex);
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
	double		now_sec;
	double		baseline_time = 0.0;
	double		last_reqtime = 0.0;
	int			ret;	
	int			i;
	int			queue_idx =0;
	pthread_t 	completion_th;
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
	if(pthread_create(&completion_th, NULL, aio_completion_handler, (void*)NULL) !=0){
		printf ("Create completion thread error!\n");
		exit(1); 
	}
//	if(pthread_detach(completion_th) !=0){
	//	printf ("Detach completion thread error!\n");
	//	exit (1);
//	}	
	int idx=0;
	for(idx=0;idx<MAXSIZE;idx++){
		// read one trace entry and initialize an aiocb
		ret = trace_nextrequest(&request,idx);
		if(ret==1)
			baseline_time += last_reqtime;
		
repeat:
		//allocate a free ioqueue
		pthread_mutex_lock (&mutex);
		for(i=0; i<QUEUE_SIZE; i++){
			this_io = &ioq[(queue_idx+i)%QUEUE_SIZE];
			if(!this_io->ref_cnt){
				queue_idx+=i+1;
				this_io->ref_cnt =1;
				break;
			}
		}
		pthread_mutex_unlock (&mutex);
		if(i == QUEUE_SIZE){
			usleep(200);
			goto repeat;
		}

		if (AIO_BLKSIZE < request.bytecount){
			printf("%d buffer is allocated for a %d request, buffer is too small\n", AIO_BLKSIZE, request.bytecount);
			exit(0);
		} 			

		request.startbyte -= request.startbyte % 512;
		this_iocb = &this_io->iocb;
		if(request.rwType == 'R' )
			io_prep_pread (this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		else
			io_prep_pwrite(this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		this_iocb->data = this_io;
	//	printf("submit iocb=%p\n",this_iocb);
		last_reqtime = request.reqtime;
		//sleep some time if necessary
		now_sec= elapse_sec();
		if(request.reqtime>now_sec){
			usleep((long)((request.reqtime-now_sec)*1000000.0));	
			//printf("usleep: %ld\n",(long)((request.reqtime-now_sec)*1000000.0));
		}

		//replay the request
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

int main(int argc, char * argv[])
{
	pthread_t listen_th;
	int i,flags,bw;
	uint64_t ctime,stime;

	srand(0);
	for(i=0;i<MAXSIZE;i++){
		offs[i]= AIO_BLKSIZE*(rand() % MAXSIZE);
	}
//SIGINT
	signal(SIGINT,sigint_handler);
	//create the listen thread
	
	flags =   O_RDWR|  O_LARGEFILE;
	raid_fd =open(argv[1],flags,0777);
	if (raid_fd < 0) {
		printf("RAID5 open failure:%d\n",raid_fd);
		exit(1) ;
	}
	stime = cur_usec();
	initialize();
	gettimeofday(&test_start,NULL);
	io_play();

	finalize();
	ctime = cur_usec();
	bw = MAXSIZE * (AIO_BLKSIZE);
	bw = bw / (ctime - stime);
	printf("bw=%"PRIu64"",bw);

	bw *= 1000000;
	bw = bw / 1024;
	printf("stat: bw = %"PRIu64" kB/s\n",bw);

	close(raid_fd);

	return 0;
}




