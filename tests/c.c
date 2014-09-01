#include<stdio.h>
#include<inttypes.h>
struct DirEntry
{
    int inode_;
    char filetype_;
    uint32_t nextoff_;
    uint16_t len_;
    char *pname_;
}__attribute__((__packed__));

typedef struct DirEntry dir_entry_t;

dir_entry_t *ll_malloc(int size){
     dir_entry_t *p = malloc(size);
     printf("p=%p\n",p);
     return p;
}
main(){
int i,size;
 dir_entry_t *p;
   for(i=0;i<1000;i++){
      p=  ll_malloc(sizeof(dir_entry_t));
      p->len_ = 10; 
     if(p->nextoff_ != -1)
     printf("after return p=%p\n",p);
      if(i %4==0)
	free(p);
   }
}
