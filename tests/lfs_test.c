#include<pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#define TEST_BLKSIZE (200<<10)

typedef struct test_unit{
	int id;
	uint64_t fpos;
}test_unit_t;
int *readfd;
int num_files;



#if 0
int lfs_test_write(char *buffer){
	int i,id;
	uint64_t offset = 0;
	id = test_create(AVG_FSIZE);
	if(id<0){
		exit(1);
	}
	for(i=0;i<200;i++){
		test_write(id,buffer,LFS_BLKSIZE,offset);
		offset += LFS_BLKSIZE;
	}
	return id;
}
int lfs_test_write_all(char *buffer){
	int id,i;
	uint64_t offset;
	while(1){
	   	id = test_create(AVG_FSIZE);
		if(id < 0){
			exit(1);
		}
		offset=0;
		for(i=0;i<200;i++){
        	file_write(id,buffer,LFS_BLKSIZE,offset);
       		offset += LFS_BLKSIZE;
    	}
		printf("finish create %d,",id);  
	}

}
#endif
void func1(){
	while(1){
	int *p=pthread_getspecific(kkey);
	printf("thread 1's key=%d",*p);}
}
void func2(){
	while(1){
	int *p=pthread_getspecific(kkey);
	printf("thread 2's key=%d",*p);
}
}

void *lfs_test_read(void *arg){
	int i;
	int nums,size=200<<10;
	int *p=1;
	pthread_setspecific(kkey,arg);
	if(*arg==1){
		func1();
	}
	else func2();
	return NULL;
}

void read_test_init(){

	readfd = malloc((10<<10)*sizeof(int));

}
#if 0
int lfs_test_randread(){
	test_unit_t ut[2000];
	int i;
	int randoff,rdx;
	readfd = malloc((10<<10)*sizeof(int));
	num_files = 0;
	lfs_getdlist(readfd);
	srand(time(0));
	
	while(1){
		for(i=0;i<2000;i++){
			randoff = rand()%(200<<10);
			rdx = rand() % num_files;
			ut[i].id = rdx;
			ut[i].fpos = randoff >TEST_BLKSIZE?(randoff - TEST_BLKSIZE):randoff;
	}

		for(i=0;i<2000;i++)
			lfs_test_read(ut[i].id,ut[i].fpos);	
	}

	free(readfd);
}
#endif
void read_test_fini(){
	free(readfd);
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

#define THREAD_NUMS 2ull
int main(){
	int i=0,randfd[THREAD_NUMS];
	uint64_t bw;
	char fname[6]={NULL};
	pthread_t tids[THREAD_NUMS];
	uint64_t stime,ctime;
	for(i=0;i<THREAD_NUMS;i++){
		randfd[i] = i;
	}
	srand(time(0));
	pthread_key_create(kkey,NULL);
	stime = cur_usec();	
	for(i=0;i<THREAD_NUMS;i++){
		
		sprintf(fname,"f%d",rand()%1000+1);
		printf("fname=%s\n",fname);
		randfd[i] = open(fname,O_RDONLY);
		if(randfd[i]==-1){
			printf("error\n");
			exit(1);
		}
		memset(fname,0,6);
    	pthread_create(&tids[i],NULL,lfs_test_read,(void *)randfd[i]);
	}
	for(i=0;i<THREAD_NUMS;i++){
    pthread_join(tids[i],NULL);
	}

//	stime = cur_usec();	
//	lfs_test_read(NULL);
	ctime = cur_usec();
	read_test_fini();
	return 1;
}













