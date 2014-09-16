#include<fcntl.h>
#include<pthread.h>
#include <sys/time.h>
#include<sys/un.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<inttypes.h>
#include<assert.h>
#define TEST_BLKSIZE (200<<10)
#define LFS_BLKSIZE (1<<20)
int *readfd;
int num_files = 0;
int max_files = 1000;


uint64_t cur_usec (void)
{
    struct timeval _time;
    unsigned long long cur_usec;

    gettimeofday (&_time, NULL);
    cur_usec = _time.tv_sec;
    cur_usec *= 1000000;
    cur_usec += _time.tv_usec;

    return cur_usec;
}


#if 0
int
lfs_test_write_all (char *buffer)
{
    int fd;
    int res, i = 0, j;
    uint64_t offset;
    char filename[25];
    for (i = 1; i < 100; i++)
      {

	  memset (filename, 0, 25);
	  sprintf (filename, "/t%d", i);
	  printf ("create file %s\n", filename);
	  res = test_create (filename);
	  if (res == -1)
	    {
		printf ("continuing\n");
		continue;
	    }

	  fd = open (filename,O_RDWR);
	  if (fd == -1 || fd == 0)
	    {
		printf ("test write failed\n");
		return -1;
	    }
	  offset = 0;
	  continue;
	  for (j = 0; j < 200; j++)
	    {
	//	write (id, buffer, LFS_BLKSIZE, offset);
		offset += LFS_BLKSIZE;
	    }
	  printf ("finish create %d,", fd);
      }

}
#endif

/* simulate applications' workload:256KB stream reads.
 */
void *
lfs_test_read (void *arg)
{
    int i, id;
    int fd,ret;
    int nums, size = 256 << 10;
    char *rbuffer, fname[10];
    nums = 1024;
    id = (int) arg;
    uint64_t offset = 0;
    rbuffer = malloc (size);
    memset (fname, 0, 10);
    memset (rbuffer, 0, size);
    sprintf (fname, "/mnt/sdb1/t%d", id);
    fd = open (fname,O_RDWR);
	assert(fd>=0);
    for (i = 0; i < 800; i++)
      {
	  ret = pread (fd, rbuffer, size, offset);
	  printf("fd=%d,off=%d\n",fd,offset);
//	  printf("read finsished %d",ret);
	  if(ret <=0){
	      printf("ret =%d\n",ret);
	      break;}
	  offset += size;
      }
//    printf ("rbuffer=%c%c%c\n", rbuffer[0], rbuffer[1], rbuffer[2]);
    free (rbuffer);
    return NULL;
}

void
read_test_fini ()
{
    free (readfd);
}
void show_time(int threads,uint64_t ctime,uint64_t stime){
    uint64_t bw;
    bw = (200 << 20);
    bw *= 200;
    bw = bw / (ctime - stime);
    printf ("bw=%" PRIu64 "", bw);

    bw *= 1000000;
    bw = bw / 1024;
    printf ("stat: bw = %" PRIu64 " kB/s\n", bw);
}
#define MAX_THREADS 500ull
/* num_files1 : test files.
 */
int
lfs_test_streamread (int num_files1, int threads)
{
    int i = 0, randfd[MAX_THREADS];
    uint64_t bw;
    pthread_t tids[MAX_THREADS];
//    read_test_init ();
    srand (time (0));
    uint64_t stime, ctime;
    for (i = 0; i < threads; i++)
      {
	  randfd[i] = rand () % num_files1;
	  if (randfd[i] == 0)
	      randfd[i] = 1;
      }
    srand (time (0));

    stime = cur_usec ();
    for (i = 0; i < threads; i++)
      {
	  printf ("going to read %d", randfd[i]);
	 
	  pthread_create (&tids[i], NULL, lfs_test_read, (void *) randfd[i]);
//	  pthread_create (&tids[i], NULL, lfs_test_inode_read, (void *) randfd[i]);

      }
    for (i = 0; i < threads; i++)
      {
	  pthread_join (tids[i], NULL);
      }
//      stime = cur_usec();     
//      lfs_test_read(NULL);
    ctime = cur_usec ();
    show_time(threads,ctime,stime);
    read_test_fini ();
    return 1;
}

void
filebench (char *argv[])
{
    int i, thds, files, size = 0;
    char *testbuffer;
    char fname[10];
    if (argv[0] == NULL || argv[1] == NULL)
      {
	  printf ("usage:./rfstool filebench r/w [threads] [files]\n");
	  return;
      }
    testbuffer = malloc (LFS_BLKSIZE);
    for (i = 0; i < LFS_BLKSIZE; i++)
      {
	  testbuffer[i] = i % 4 + '0';
      }

    if (strcmp (argv[1], "f") == 0)
      {
	  if (argv[2] == 0)
	    {
		printf ("usage: ./client filebench fallocate [files]\n");
		return ;
	    }
	  files = atoi (argv[2]);
	  for (i = 0; i < files; i++)
	    {
		size = 200 << 20;
		memset (fname, 0, 10);
		sprintf (fname, "/mnt/sdb1/t%d", i);
  	        int fd = open(fname, O_RDWR| O_CREAT);
		if(fd <=0){
			printf("create %s failed\n",fname);
			return ;
		}
		if (fallocate (fd,0,0,size)!=0)
		    printf ("no space in rfs when allocate %s\n", fname);
		close(fd);
	    }
      }
    else if (strcmp (argv[1], "r") == 0)
      {
	  if (argv[2] == NULL || argv[3] == NULL)
	    {
		printf ("usage:./rfstool filebench r/w [threads] [files]\n");
		return;
	    }
// default thds = 200
	  thds = atoi (argv[2]);
	  files = atoi (argv[3]);

	  lfs_test_streamread (files, thds);

      }
    else if (strcmp (argv[1], "w") == 0)
      {
	 // lfs_test_write_all (testbuffer);
      }
    else
	printf ("invalid args\n");

}

int
main (int argc, char *argv[])
{
    if (argv[1] == '\0')
      {
	  return 0;
      }

    filebench (argv + 1);
      
    return 0;
}
