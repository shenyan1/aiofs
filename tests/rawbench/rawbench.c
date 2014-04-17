#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE 
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
#define TRACE_FILE 			"trace.txt"
#define COMPLETE_TIME_FILE	"complete_time.txt"
#define RESPONSE_TIME_FILE	"response_time.txt"

FILE *tracefile;
int	raid_fd;
FILE *compfile;
FILE *respfile;
//long		period_nr=0;
const char * trace_name = TRACE_FILE;
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


int initialize()
{
	if ((tracefile = fopen(trace_name,"rb")) == NULL) {
		fprintf(stderr, "Tracefile %s cannot be opened for read \n", TRACE_FILE);
		exit(1);
	}

	if ((compfile= fopen(COMPLETE_TIME_FILE,"a")) == NULL) {
		fprintf(stderr, "Complete time file %s cannot be opened for append\n", COMPLETE_TIME_FILE);
		exit(1);
	}

	if ((respfile= fopen(RESPONSE_TIME_FILE,"a")) == NULL) {
		fprintf(stderr, "response time file %s cannot be opened for append\n", RESPONSE_TIME_FILE);
		exit(1);
	}

	pthread_mutex_init (&mutex,NULL);
	
	return 1;
}

/*return value:
 *-1: failed
 *  0: successful
 *  1: successful and another round
 */
int trace_nextrequest(struct trace_entry* req)
{
	
	char line[201];
	unsigned int u_devno;
	int	ret = 0;
	
	if (fgets(line, 200, tracefile) == NULL) {
		if(feof(tracefile)){
			clearerr(tracefile);
			fseek(tracefile, 0L, SEEK_SET);				
			ret = 1;
		}
		
		if (fgets(line, 200, tracefile) == NULL) {
			fprintf(stderr, "Tracefile is NULL\n");
		   	return  -1;
		}		
	}
	
	if (sscanf(line, "%u, %lld, %d, %c, %lf\n", &u_devno,&req->startbyte,&req->bytecount,&req->rwType, &req->reqtime) != 5) {
		fprintf(stderr, "Wrong number of arguments for I/O trace event type\n");
		return -1;
	}
	req->devno = id2no(u_devno);
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

 
int finalize(	)
{
	
	fclose(tracefile);
	fclose(compfile);
	fclose(respfile);
	
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

	while(1){
		num = io_getevents(ioctx, 1, AIO_MAXIO, events, NULL);
		if(should_stop)
			break;
		printf("\n%d io_request completed\n\n", num);

		comp_time =elapse_sec();
		for(i=0;i<num;i++){
			this_io = (struct io_queue*)events[i].data;
			resp_time = comp_time - this_io->issue_time;

			if (events[i].res2 != 0) {
				printf("aio write error \n");
			}
			if (events[i].obj != &this_io->iocb) {
				printf("iocb is lost \n");
				exit(1);
			}

			if (events[i].res != this_io->iocb.u.c.nbytes) {
				printf("rw missed bytes expect % ld got % ld \n", this_io->iocb.u.c.nbytes, events[i].res);
				exit(1);
			}
					
			// Request completed successfully
			//sample response time, write into file if needed
			complete_time[record_count] =comp_time;
			response_time[record_count]= (long)( resp_time *1000000.0) ;
			if (response_time[record_count]==0)
				printf("zero!!\n");
			record_count++;
			if(record_count==65536){
				for(j=0;j<record_count;j++){
					fprintf(compfile,"%lf\n", complete_time[i]);
					fprintf(respfile,"%ld\n", response_time[i]);
				}
				record_count=0;
			}
		}

		pthread_mutex_lock (&mutex);
		for(i=0;i<num;i++){
			this_io = (struct io_queue*)events[i].data;
			this_io->ref_cnt =0;
		}
		pthread_mutex_unlock (&mutex);
	}
	
	pthread_exit(NULL) ;
}


int 				req_count=0;
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
	if(pthread_detach(completion_th) !=0){
		printf ("Detach completion thread error!\n");
		exit (1);
	}	
	
	while (1){
		// read one trace entry and initialize an aiocb
		ret = trace_nextrequest(&request);
		if(ret==1)
			baseline_time += last_reqtime;
		
		if(!(request.devno< REFERENCE_DISKS)){
			continue;
		}
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
			io_prep_pread(this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		else
			io_prep_pwrite(this_iocb, raid_fd, this_io->buf, request.bytecount, request.startbyte);
		this_iocb->data = this_io;

		last_reqtime = request.reqtime;
		//sleep some time if necessary
		request.reqtime+=baseline_time;  //fix the trace period
		now_sec= elapse_sec();
		if(request.reqtime>now_sec){
			usleep((long)((request.reqtime-now_sec)*1000000.0));	
			//printf("usleep: %ld\n",(long)((request.reqtime-now_sec)*1000000.0));
		}

		this_io->issue_time = elapse_sec();
		//replay the request
		ret = io_submit(ioctx, 1, &this_iocb);
		printf("iocb->");
		if(ret < 0)
			printf("io_submit io error\n");
		else
			printf("\nsubmit  %d  read request\n", ret);
		
		req_count++;
		if (should_stop)
			break;

	}
	free(ioq);
}

//this thread will listen to the STOP-TEST command from user or a snoopy process
void *listen_thread(void * ptr)
{	
	int sock;
	struct sockaddr_in name;
	int len = sizeof(struct sockaddr_in);
	long rqst;

	// for communication		
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		printf("open socket error\n");
		exit(1);
	}
	
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = inet_addr("10.0.2.161");//INADDR_ANY;
	name.sin_port = htons(6666);
	
	if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0) {
		perror("bind");
		exit(1);
	}

	do{
		recvfrom(sock, &rqst, sizeof(rqst), MSG_WAITALL, (struct sockaddr*)&name, (socklen_t*)&len);
	}while(rqst!=6666);

	close(sock);
	should_stop=1;
	
	pthread_exit(NULL) ;
	
}

int main(int argc, char * argv[])
{
	pthread_t listen_th;
	int i,flags;

	if(argc>1){
		trace_name = argv[1];
	}

	complete_time = malloc(sizeof(double)*65536);//65536 is chosen according to /proc/sys/fs/aio_max_nr 
	response_time = malloc(sizeof(long)*65536);

	//create the listen thread
	if(pthread_create(&listen_th, NULL, listen_thread, (void*)NULL) !=0){
		printf ("Create listen thread error!\n");
		exit(1); 
	}
	if(pthread_detach(listen_th) !=0){
		printf ("Detach listen thread error!\n");
		exit (1);
	}

	
	flags =   O_RDWR|  O_LARGEFILE|O_DIRECT;
	if(argc>2)
		raid_fd =open(argv[2],flags,0777);
	else
		raid_fd= open("/dev/sdh1", flags, 0777);
	if (raid_fd < 0) {
		printf("RAID5 open failure:%d\n",raid_fd);
		exit(1) ;
	}
	//printf("raid fd=%d\n",raid_fd);
	initialize();
	gettimeofday(&test_start,NULL);
	io_play();

	for(i=0;i<=record_count;i++){
		fprintf(compfile,"%lf\n", complete_time[i]);
		fprintf(respfile,"%ld\n", response_time[i]);
	}
	fprintf(compfile,"request count=%d\n", req_count);

	free(complete_time);
	free(response_time);
	finalize();
	close(raid_fd);

	return 0;
}




