#include<fcntl.h>
#include<stdio.h>
main(){
 int fd,num=4;
 fd = open ("fs", O_CREAT | O_WRONLY | O_APPEND, 0600);
 write(fd,&num,sizeof(num));
 close(fd);
}
