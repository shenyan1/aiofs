//#include"../lfs.h"
//#include"../lfs_ops.h"
#include<pthread.h>
#include <sys/time.h>
#include "rfsio.h"
#include<sys/un.h>
#include<stdio.h>
#include<unistd.h>
#include<lfs_define.h>
#include<stdlib.h>
#define TEST_BLKSIZE (200<<10)

int *readfd;
int num_files = 0;

int
test_create (char *fname)
{
    int id;
    if ((id = rfs_create (fname)) == LFS_FAILED)
      {
	  _cli_printf ("create failed\n");
	  return -1;
      }
    return id;
}

int
test_write (int id, char *buffer, uint64_t size, uint64_t offset)
{


    if (rfs_write (id, buffer, size, offset))
      {
	  _cli_printf ("write failed id=%d,size=%" PRIu64 ",offset=%" PRIu64
		      "", id, size, offset);
	  exit (1);
      }
    return 1;
}

int
test_read (int id, char *buffer, uint64_t size, uint64_t offset)
{
    if (rfs_read (id, buffer, size, offset))	//file_read (id, buffer, size, offset))
      {
	  _cli_printf ("read failed id=%d,size=%" PRIu64 ",offset=%" PRIu64 "",
		      id, size, offset);
	  return -1;
      }

//    printf ("read buf=%c%c%c", buffer[0], buffer[1], buffer[2]);
    return 0;
}

int
lfs_test_write_all (int files,char *buffer)
{
    inode_t id;
    int res, i = 0, j,size=256<<10;
    uint64_t offset;
    char filename[25];
    for (i = 1; i <= files; i++)
      {

	  memset (filename, 0, 25);
	  sprintf (filename, "/t111%d", i);
	  _cli_printf ("create file %s\n", filename);
	  res = rfs_create (filename);
	  if (res == -1)
	    {
		_cli_printf ("create failed\n");
	    }

	  id = rfs_open (filename);
	  if (id == LFS_FAILED || id == 0)
	    {
		_cli_printf ("test write failed\n");
		return -1;
	    }
	  printf ("fname=%s,inode=%d\n", filename, id);
	  offset = 0;
	 // continue;
#if 0
	  for (j = 0; j < 200; j++)
	    {
		if(rfs_write (id, buffer, LFS_BLKSIZE, offset)==-1){
			printf("error rfs write return -1\n");
			exit(-1);
	        }
		offset += LFS_BLKSIZE;
	    }
#else
	printf("going to write file");
	  for (j = 1; j <= 840; j++)
	    {
		if(rfs_write (id, buffer, size, offset)==-1){
			printf("error rfs write return -1\n");
			exit(-1);
	        }
		offset += size;
	    }
#endif
	  printf ("finish create %d\n", id);
	  rfs_close(id);
      }

}

#if 0
void *
lfs_test_inode_read (void *arg)
{
    int i, id;
    inode_t inode;
    int nums, size = 256 << 10;
    char *rbuffer, fname[10];
    nums = 1024;
    inode = id = (int) arg;
    uint64_t offset = 0;
    rbuffer = malloc (size);
    memset (rbuffer, 0, size);
    for (i = 0; i < 80; i++)
      {
	  if (test_read (inode, rbuffer, size, offset))
	      break;

	  offset += size;
      }
    _cli_printf ("rbuffer=%c%c%c\n", rbuffer[0], rbuffer[1], rbuffer[2]);
    free (rbuffer);
    return NULL;
}
#endif
/* simulate applications' workload:256KB stream reads.
 */
//#define _LFS_TEST_MULTBLOCK 1
void *
lfs_test_read (void *arg)
{
    int i, id;
    inode_t inode;
    int nums, size = 256 << 10;
    char *rbuffer, fname[10];
    nums = 1024;
    id = (int) arg;
    uint64_t offset = 0;
    rbuffer = malloc (size);
    memset (fname, 0, 10);
    memset (rbuffer, 0, size);
    sprintf (fname, "/t%d", id);
    inode = rfs_open (fname);
    if (inode <= 0)
      {
	  _cli_printf ("test_read failed\n");
	  return 0;
      }
#ifdef _LFS_TEST_MULTBLOCK
    offset = LFS_BLKSIZE - (128<<10) ;
    test_read(inode , rbuffer, size, offset);
    printf("rbuffer=%s\n",rbuffer);
    free(rbuffer);
    return 0;
#endif
    for (i = 0; i < 800; i++)
      {
	  if (test_read (inode, rbuffer, size, offset))
	      break;

//          printf ("rbuffer=%c%c%c\n", rbuffer[0], rbuffer[1], rbuffer[2]);
	  offset += size;
      }
    free (rbuffer);
    return NULL;
}

#if 0
int
lfs_test_randread ()
{
    test_unit_t ut[2000];
    int i;
    int randoff, rdx;
    readfd = malloc ((10 << 10) * sizeof (int));
    num_files = 0;
    lfs_getdlist (readfd);
    srand (time (0));

    while (1)
      {
	  for (i = 0; i < 2000; i++)
	    {
		randoff = rand () % (200 << 10);
		rdx = rand () % num_files;
		ut[i].id = rdx;
		ut[i].fpos =
		    randoff >
		    TEST_BLKSIZE ? (randoff - TEST_BLKSIZE) : randoff;
	    }

	  for (i = 0; i < 2000; i++)
	      lfs_test_read (ut[i].id, ut[i].fpos);
      }

    free (readfd);
}
#endif
void
read_test_fini ()
{
    free (readfd);
}

void
show_time (int threads, uint64_t ctime, uint64_t stime)
{
    uint64_t bw;
    bw = (200 << 20);
    bw *= threads;
    bw = bw / (ctime - stime);
//    printf ("bw=%" PRIu64 "", bw);
    printf("ctime = %"PRIu64",stime=%"PRIu64"\n",ctime,stime);
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
    pthread_t tids[MAX_THREADS];
    // read_test_init ();
    srand (time (0));
    uint64_t stime, ctime;
    if (num_files < num_files1)
      {
	  _cli_printf ("num_files=%d %d numfiles < requested file nums\n",
		      num_files, num_files1);
      }
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
	  _cli_printf ("going to read %d,", randfd[i]);
	  pthread_create (&tids[i], NULL, lfs_test_read, (void *) randfd[i]);
//        pthread_create (&tids[i], NULL, lfs_test_inode_read, (void *) randfd[i]);

      }
    for (i = 0; i < threads; i++)
      {
	  pthread_join (tids[i], NULL);
      }

//      stime = cur_usec();     
//      lfs_test_read(NULL);
    ctime = cur_usec ();
    show_time (threads, ctime, stime);
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
	  _cli_printf ("usage:./rfstool filebench r/w [threads] [files]\n");
	  return;
      }
    testbuffer = malloc (LFS_BLKSIZE);
    for (i = 0; i < LFS_BLKSIZE; i++)
      {
	  testbuffer[i] = i % 4 + '2';
      }

    if (strcmp (argv[1], "f") == 0)
      {
	  if (argv[2] == 0)
	    {
		_cli_printf ("usage: ./client filebench fallocate [files]\n");
		return;
	    }
	  files = atoi (argv[2]);
	  for (i = 0; i < files; i++)
	    {
		size = 200 << 20;
		memset (fname, 0, 10);
		sprintf (fname, "/t%d", i);
		if (rfs_fallocate (fname, size) == LFS_FAILED)
		    _cli_printf ("no space in rfs when allocate %s\n", fname);
	    }
      }
    else if (strcmp (argv[1], "r") == 0)
      {
	  if (argv[2] == NULL || argv[3] == NULL)
	    {
		_cli_printf
		    ("usage:./rfstool filebench r/w [threads] [files]\n");
		return;
	    }
// default thds = 200
	  thds = atoi (argv[2]);
	  files = atoi (argv[3]);

	  lfs_test_streamread (files, thds);

      }
    else if (strcmp (argv[1], "w") == 0)
      {
	  if(argv[2]== NULL){
		printf("usage: ./lfs_load filebench w files\n");
		return ;
	  }
	  files = atoi(argv[2]);
	  lfs_test_write_all (files,testbuffer);
      }
    else
	_cli_printf ("invalid args\n");

}


int
funcbench ()
{
    rfs_create ("a.doc");
    rfs_rmdir ("/a/");
    rfs_mkdir ("/shenyan");
    rfs_fallocate ("a.doc", 200 << 20);
    rfs_fallocate ("a1.doc", 256 << 20);
    return 0;
}

int
main (int argc, char *argv[])
{
    if (argv[1] == '\0')
      {
	  printf ("usage:[op]+[name]\n"
		  "op:rmdir,mkdir,filebench,stopfs\n"
		  "rmdir:  remove a dir\n"
		  "rm:     remove a file\n"
		  "filebench: filebench w (r threads files)test rfs io throughput\n"
		  "functionbench: test rfs function\n"
		  "mkdir:  create a dir\n"
		  "stopfs: close rfs filesystem\n"
		  "cp: copy file from rfs to normal filesystem\n"
		  "ls:     list files\n");
	  return 0;
      }
    if (strncmp (argv[1], "rmdir", strlen ("rmdir")) == 0)
      {
	  if (rfs_rmdir (argv[2]) == -1)
	    {
		_cli_printf ("rm dir %s failed\n", argv[2]);
	    }
      }
    else if (strncmp (argv[1], "mkdir", strlen ("mkdir")) == 0)
      {
	  if (rfs_mkdir (argv[2]) == -1)
	    {
		_cli_printf ("create dir %s failed\n", argv[2]);
	    }

      }
    else if (strncmp (argv[1], "stopfs", strlen ("stopfs")) == 0)
      {
	  stopfs ();
      }
    else if (strncmp (argv[1], "ls", strlen ("ls")) == 0)
      {
	  lsfs (argv + 2);
      }
    else if (strncmp (argv[1], "cp", strlen ("cp")) == 0)
      {

      }
    else if (strncmp (argv[1], "filebench", strlen ("filebench")) == 0)
      {
	  filebench (argv + 1);
      }
    else if (strncmp (argv[1], "rm", strlen ("rm")) == 0)
      {
	  if (rfs_remove (argv[2]) == -1)
	    {
		_cli_printf ("remove a file failed\n");
	    }
      }
    else if(strcmp(argv[1],"funcbench")==0){
	 funcbench();
    }
    return 0;
}
