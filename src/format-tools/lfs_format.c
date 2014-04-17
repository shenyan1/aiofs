/*
   To format the whole disk.
   The disk format is :Name（8B）	Version8B	FileNums（8B）
 */
#include<stdio.h>
#include<assert.h>
#include<stdlib.h>
#include<inttypes.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"lfs.h"
uint64_t tot_size=0;
uint64_t getphymemsize(){
    return sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
}
uint64_t getFilesize(char *str){
FILE *fp=fopen(str,"r");
fseek(fp,0L,SEEK_END);
uint64_t size=ftell(fp);
fclose(fp);
return size;
}
/*
  The FS information have 8B
 */
int FormatFSinformation(int fd)
{
	int n;
	n=sizeof(FSNAME)+sizeof(VERSION);
	printf("n=%d",n);
	write(fd,FSNAME,sizeof(FSNAME));
	write(fd,VERSION,sizeof(VERSION));
}
/*
  The FileEntry have (57+16)*10KB = 730KB
*/
int FormatFileEntry(int fd)
{
	int i=0;
	char *buffer = malloc(FILE_ENTRYS*8);
	memset(buffer,0,FILE_ENTRYS*8);
	uint64_t disk_off,max_files,size,tmpoff[2];
	uint64_t offset = LFS_DATA_DOMAIN;
	size = tot_size - offset ;
	max_files = size / AVG_FSIZE;
	disk_off = LFS_FILE_ENTRY;
/*
        16 is the format of disk :8B for filename 4B:filesize 4B for padding
 */
	tmpoff[0] = offset;
	for(i=0;i<MAX_FILE_NO;i++){
		pwrite(fd,buffer,16,disk_off);

		disk_off +=16;

		if(i<max_files){
			printf("file %d off=%"PRIu64"\n",i,tmpoff[0]);
		//	write(fd,&tmpoff,sizeof(uint64_t));
			pwrite(fd,tmpoff,sizeof(uint64_t),disk_off);
			tmpoff[0] =  tmpoff[0] + AVG_FSIZE;
			pwrite(fd,buffer,(FILE_ENTRYS-1)*sizeof(uint64_t),disk_off+sizeof(uint64_t));
			disk_off += FILE_ENTRYS*sizeof(uint64_t);
		}else {

			pwrite(fd,buffer,(FILE_ENTRYS)*sizeof(uint64_t),disk_off);
			disk_off += FILE_ENTRYS*sizeof(uint64_t);
		}
	}
	free(buffer);
	return 0;	
}
/*
 * This method is to build small spacemap. We put the reserved space into small spacemap.
 *  The large fs support entrys <=1M entrys. So setup the entries here.
 */
int FormatSpaceEntry(int fd)
{
   uint64_t i,offset,size;
   char *zero;
   uint64_t r_sizes;
   uint64_t ptr[2];
   uint32_t max_files;
   offset = LFS_DATA_DOMAIN;
   size =tot_size-offset;
   zero = malloc(16<<20);
   memset(zero,0,16<<20);
   printf("off=%"PRIu64",size=%"PRIu64"\n",offset,size);
   max_files = size / AVG_FSIZE ;
   if(size % AVG_FSIZE == 0)
		r_sizes = 0;
   else r_sizes = size - (size / AVG_FSIZE)*AVG_FSIZE;
   offset = (size / AVG_FSIZE) * AVG_FSIZE;
   ptr[0] = offset;
   ptr[1] = r_sizes;
   r_sizes /= (1024*1024);
   printf("It can contains %d 200M files,and reserves %"PRIu64"M size \n",max_files,r_sizes);
   pwrite(fd,&max_files,sizeof(uint32_t),sizeof(uint64_t));
   pwrite(fd,ptr,2*sizeof(uint64_t),LFS_SPACE_ENTRY);
   pwrite(fd,zero,(16<<20-16),LFS_SPACE_ENTRY+2*sizeof(uint64_t));
   free(zero);
}

main(int argc, char *argv[])
{
	int fd;
	int flag = O_WRONLY | O_CREAT;
	printf("Going to Format LFS now\n");
	printf("the totoal memsize is %"PRIu64"\n",getphymemsize());
	tot_size = getFilesize(argv[1]);
	printf("totsize=%"PRIu64"\n",tot_size);
	if(argv[1]==NULL){
		printf("The LFS request a block device\n ");
		exit(1);
	}
        fd = open(argv[1],flag,0666);	
	FormatFSinformation(fd);
	FormatFileEntry(fd);
	FormatSpaceEntry(fd);
	close(fd);
}	
