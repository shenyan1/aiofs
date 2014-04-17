#include<stdio.h>
main()
{
	char *p;
	p = malloc(10);
	printf("%s",p);
	free(p);
	free(p);
}
