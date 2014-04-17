#include<inttypes.h>
#include<stdlib.h>
#include<stdio.h>
main()
{
  char *buf;
	buf = malloc(10);
	*buf='2';
  printf("sizeof%d",strlen(buf));
  printf("sizeof%d",sizeof(uint16_t));
}
