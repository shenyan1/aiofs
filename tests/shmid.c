#include<stdio.h>
main(){
   char num[5];
   int *ptr;
   ptr = num;
   *ptr = 1234;
   printf("%x | %x| %x| %x",num[0],num[1],num[2],num[3]);
 }
