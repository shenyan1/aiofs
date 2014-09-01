#define _GNU_SOURCE 1
#include<stdio.h>
#include<fcntl.h>
#include<sys/shm.h>
main(){
 int id,ret,i;
 int size=1<<10;
 char *rbuffer,*ptr,ptr2;
 ret = posix_memalign((void **)&rbuffer, 4096, size);
  printf("mem rbuf =%p",rbuffer);
  id=shmget(IPC_PRIVATE,size,(SHM_R|SHM_W|IPC_CREAT));
 ptr = shmat(id,NULL,0);
 *ptr='2';
 printf("ptr=%p",ptr);
 *ptr='1';
 ret = open("/dev/sdg",O_DIRECT);
 read(ret,ptr,size);
 printf("buf=%s",ptr);
 printf("after align ptr=%p,%c\n",++ptr,*ptr);
 for(i=0;i<4096;i++){
     printf("%c",ptr[i]);
 }
 
 shmctl(id,IPC_RMID,NULL);
 free(rbuffer);
}
