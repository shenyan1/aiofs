#include<stdio.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<inttypes.h>
#include<fcntl.h>
#include<pthread.h>
#include <sys/time.h>
#include<inttypes.h>
#define TEST_BLKSIZE (200<<10)
#define AIO_BLKSIZE 512*256
#define THREAD_NUMS 10ull
#define MAXSIZE 10000
typedef struct test_unit{
	int id;
	uint64_t fpos;
}test_unit_t;
int *readfd;
int num_files=0;
int fd;
int offs[MAXSIZE];
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
void *lfs_test_read(void *arg){
   int i,n = (int )arg,size=AIO_BLKSIZE;
   static int c=0;
   char buffer[AIO_BLKSIZE+1];
   c++;
   for(i=0;i<MAXSIZE/THREAD_NUMS;i++){
//	printf("offset=%d,%p\n",i+n*MAXSIZE/THREAD_NUMS,(void *)pthread_self());
   	int it=pread(fd,buffer,size,offs[i+n*MAXSIZE/THREAD_NUMS]);
//	printf("fd=%d pthreads return %d\n",fd,it);
  }
   return NULL;
}

int lfs_test_streamread(){
	int i=0;
	uint64_t bw;
	pthread_t tids[THREAD_NUMS];
	srand(time(0));
	uint64_t stime,ctime;
        fd=open("/dev/sdb",O_RDWR,0666);
	printf("fd=%d",fd);
	srand(0);
	for(i=0;i<MAXSIZE;i++){
		offs[i]= AIO_BLKSIZE*(rand() % MAXSIZE);
	}

	stime = cur_usec();	
	for(i=0;i<THREAD_NUMS;i++){
		printf("going to read %d\n",fd);
    		pthread_create(&tids[i],NULL,lfs_test_read,(void *)i);
	}
	for(i=0;i<THREAD_NUMS;i++){
 	        pthread_join(tids[i],NULL);
	}

	ctime = cur_usec();
	bw = AIO_BLKSIZE*MAXSIZE;
	bw = bw / (ctime - stime);
	printf("bw=%"PRIu64"",bw);

	bw *= 1000000;
	bw = bw / 1024;
	printf("stat: bw = %"PRIu64" kB/s\n",bw);
	return 1;
}
main(){
lfs_test_streamread();
}
