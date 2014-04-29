#include<stdio.h>
#include<assert.h>
#include<inttypes.h>
main()
{
	int i;
	uint64_t *p;
	p = (uint64_t *)malloc(sizeof(uint64_t)*16<<20);
	memset(p,0,sizeof(uint64_t)*16<<20);
	for(i=0;i<160<<20;i++){
		if(p[i]==1)
			assert(0);
	}
	free(p);
}
