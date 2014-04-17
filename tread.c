/*
 we will make random writes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <linux/fs.h>
#include <sys/resource.h>
#include <sys/mman.h>

#include <poll.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include<sys/types.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<inttypes.h>
#include<fcntl.h>
#include<linux/fs.h>
#define SIZE 4096
#define COUNT 1000000
#define TIME_OUT 300000000

#define BLK_SIZE 4096
int chk_file_num;
typedef struct
{
    char filename[256];
    int  len;
}CHK_FILE;



#define FILE_MAX  (100000)
typedef struct test_unit{
	int fd;
	long long fpos;
}test_unit_t;
int testnum;
test_unit_t testset[2000];

CHK_FILE file_tbl[FILE_MAX] = {0};
void setup_io(int i){
	int fd,index,randnum;
	long long filepos;
	srand(time(0));
	randnum = rand();
	fd = open(file_tbl[index].filename,O_RDONLY);
	if(fd < 0){
		printf("open file error!!\n");
		exit(-2);
	}
	srand(randnum);
	randnum = rand();
	index = chk_file_num * randnum / RAND_MAX;
	filepos = file_tbl[index].len * randnum / RAND_MAX ;
	filepos  /= BLK_SIZE;
	filepos *= BLK_SIZE;
	testset[i].fd = fd;
	testset[i].fpos = filepos;
}

int add_file_to_tbl(char * name)
{
    DIR * dp;
    struct stat statbuf;
    struct dirent *dirp;
    char fullname[256];

    if (!name)
        return -1;

    if (NULL == (dp = opendir(name)))
        return -2;

    while (dirp = readdir(dp))
    {
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, ".."))
            continue;

        strcpy(fullname, name);
        strcat(fullname, "/");
        strcat(fullname, dirp->d_name);

        if (stat(fullname, &statbuf) < 0)
        {
            closedir(dp);
            return -3;
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            add_file_to_tbl(fullname);
        }
        else
        {
            strcpy(file_tbl[chk_file_num].filename, fullname);
            file_tbl[chk_file_num].len = statbuf.st_size-BLK_SIZE;
            chk_file_num ++;
        }
    }
    
    return 0;
}




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
	testnum = 0;
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
	fd = open(argv[1],flag);
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
