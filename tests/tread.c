/*
 we will make random writes.
 */
#include<sys/types.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<inttypes.h>
#include<fcntl.h>
#define SIZE 4096
#define COUNT 1000000
#define TIME_OUT 300000000
uint64_t getFilesize(char *str){
FILE *fp=fopen(str,"r");
fseek(fp,0L,SEEK_END);
uint64_t size=ftell(fp);
fclose(fp);
return size;
}
unsigned long long cur_usec(void)
{
    struct timeval __time;
    unsigned long long cur_usec;

    gettimeofday(&__time, NULL);
    cur_usec =  __time.tv_sec;
    cur_usec *= 1000000;
    cur_usec += __time.tv_usec;

    return cur_usec;
}
main(int argc, char *argv[])
{
	uint64_t itx;
	int fd,ret,flag = O_RDONLY;
	char *fn;
	char tmp[4]="123\n";
	char *buf;
	uint64_t stime,usec;
	uint64_t totalsize=0;
	unsigned long offset=0;
	struct timeval tv_begin,tv_end;
	clock_t start,end;
	double dur,speed=1.0;
	srand(time(0));
	buf = malloc(SIZE);
	for(itx=0;itx<SIZE-4;itx+=4){
	memcpy(buf+itx,tmp,4);
	}
	uint64_t size=0;
	size = getFilesize(argv[1]);
	fd = open(argv[1],flag,0666);
	if(fd == -1)
		perror("open output file");
	size = size - SIZE-1;
	gettimeofday(&tv_begin,NULL);
	stime = cur_usec();
	while(1){
		offset = rand()%size;
		ret =pread(fd,buf,SIZE,offset);
		totalsize += SIZE;
		if(ret != SIZE){
			printf("err: copy size = %d",ret);
			return -1;
		}
   usec = cur_usec();
           if (usec - stime > 300000000) break;
            
	}
	close(fd);
	gettimeofday(&tv_end,NULL);
	totalsize /= (1024*1024);
	printf("it costs %llu sec time,speed = %fMB/s\n",usec -stime,totalsize*1.0/300);
	free(buf);

	return 0;
}
