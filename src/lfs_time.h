#ifndef LFS_TIME_H
#define LFS_TIME_H
#define	ddi_get_lbolt()		(gethrtime() >> 23)
#define hz 119
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef NANOSEC
#define NANOSEC		1000000000
typedef long long hrtime_t;
hrtime_t gethrtime(void)
{
	struct timespec ts;
	int rc;

	rc = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (rc) {
		fprintf(stderr, "Error: clock_gettime() = %d\n", rc);
	        abort();
	}

	return (((u_int64_t)ts.tv_sec) * NANOSEC) + ts.tv_nsec;
}
#endif
































#endif
