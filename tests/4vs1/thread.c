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

int *readfd;


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
   int i,fd = (int )arg,size=4096;
   char buffer[4097];
   int c=0;
   srand(time(0));
   uint64_t offset=100000;
   offset=offset+4096;
   printf("offset=%"PRIu64"\n",offset);
   for(i=0;i<400000;i++){
   	pread(fd,buffer,size,offset+i*size);
  }
 
   return NULL;
}
#define THREAD_NUMS 1ull
int lfs_test_streamread(){
	int fd,i=0;
	uint64_t bw;
	pthread_t tids[THREAD_NUMS];
	srand(time(0));
	uint64_t stime,ctime;
        fd=open("/dev/sdb",O_RDWR,0666);
	if(fd==-1)
           perror("open file \n");
	printf("fd  =  %d", fd);
	stime = cur_usec();	
	for(i=0;i<THREAD_NUMS;i++){
		printf("going to read %d\n",fd);
    	pthread_create(&tids[i],NULL,lfs_test_read,(void *)fd);
	}
	for(i=0;i<THREAD_NUMS;i++){
    pthread_join(tids[i],NULL);
	}

//	stime = cur_usec();	
//	lfs_test_read(NULL);
	ctime = cur_usec();
	//bw = THREAD_NUMS*(100000<<12);
	bw = (100000<<12);
	bw = bw / (ctime - stime);
	printf("bw=%"PRIu64"",bw);

	bw *= 1000000;
	bw = bw / 1024;
	printf("stat: bw = %"PRIu64" kB/s\n",4*bw);
	return 1;
}
main(){
lfs_test_streamread();
}
