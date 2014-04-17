#include"lfs.h"
#include"lfs_ops.h"
#include<pthread.h>
#include <sys/time.h>
extern lfs_info_t lfs_n;
#define TEST_BLKSIZE (200<<10)

typedef struct test_unit{
	int id;
	uint64_t fpos;
}test_unit_t;
int *readfd;
int num_files=0;

int test_create(uint64_t size){
	int id;
	if((id=file_create(size))<0){
		printf("create failed\n");
		return -1;
	}
	return id;
}

int test_write(int id,char *buffer,uint64_t size,uint64_t offset){
	

	if(file_write(id,buffer,size,offset)){
		printf("write failed id=%d,size=%"PRIu64",offset=%"PRIu64"",id,size,offset);
		exit(1);
	}
	return 1;
}
int test_read(int id,char *buffer,uint64_t size,uint64_t offset){
	if(file_read(id,buffer,size,offset)){
        printf("read failed id=%d,size=%"PRIu64",offset=%"PRIu64"",id,size,offset);
		return -1;
    }
	return 0;
}
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
void *lfs_test_read(void *arg){
	int i;
	int nums,size=200<<10;
	char *rbuffer;
	nums = 1024;
	int id;
	id =(int)arg;
	uint64_t offset = 0;
	rbuffer = malloc(size);
	memset(rbuffer,0,size);
	for(i=0;i<1024;i++){
		if(test_read(id,rbuffer,size,offset))
			break;
	//	printf("i=%d\nrbuffer =%c",i,rbuffer[0]);	
		offset += size;
	}
	free(rbuffer);
	return NULL;
}
int lfs_getdlist(int *readfd){
	int i;
	for(i=0;i<lfs_n.max_files;i++){
		if(lfs_n.f_table[i].is_free != LFS_FREE){
			readfd[num_files++] = i;				
		}
	}
	return 0;
}

void read_test_init(){

	readfd = malloc((10<<10)*sizeof(int));
	lfs_getdlist(readfd);

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

#define THREAD_NUMS 300ull
int lfs_test_streamread(){
	int i=0,randfd[THREAD_NUMS];
	uint64_t bw;
	pthread_t tids[THREAD_NUMS];
	read_test_init();
	srand(time(0));
	uint64_t stime,ctime;
	for(i=0;i<THREAD_NUMS;i++){
		randfd[i] = rand() % num_files;
		if(randfd[i]==0)
			randfd[i]=1;
	}
	srand(time(0));

	stime = cur_usec();	
	for(i=0;i<THREAD_NUMS;i++){
		printf("going to read %d\n",randfd[i]);
    	pthread_create(&tids[i],NULL,lfs_test_read,(void *)randfd[i]);
	}
	for(i=0;i<THREAD_NUMS;i++){
    pthread_join(tids[i],NULL);
	}

//	stime = cur_usec();	
//	lfs_test_read(NULL);
	ctime = cur_usec();
	bw = THREAD_NUMS*(200<<20);
	bw = bw / (ctime - stime);
	printf("bw=%"PRIu64"",bw);

	bw *= 1000000;
	bw = bw / 1024;
	printf("stat: bw = %"PRIu64" kB/s\n",bw);
	read_test_fini();
	return 1;
}













