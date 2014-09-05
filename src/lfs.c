#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<inttypes.h>
#include<sys/types.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"lfs.h"
#include"arc.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"lfs_sys.h"
#include<stdarg.h>
#include<sys/resource.h>
#include<signal.h>
#include<sys/shm.h>
#include"lfs_fops.h"
/* To get the dnode's metadata's location
 */
extern lfs_info_t lfs_n;
//#if 0 
/* The inode range from 1
 */
//#endif 
/* rfs_return_data: return the protocol (error code or data) to client.
 * 
 */
int rfs_return_data (char *proto, int len, int clifd)
{

    if (write (clifd, proto, len) < 0)
      {
	  lfs_printf ("write error:send_request\n");
	  return (-1);
      }
    return LFS_SUCCESS;
}

uint64_t arc_hash (uint64_t id, uint64_t offset)
{

    uint64_t crc = -1;

    crc ^= (id >> 8) ^ offset;
    return crc;
}

void lfs_mutex (pthread_mutex_t * lock)
{
    pthread_mutex_lock (lock);
}

void lfs_unmutex (pthread_mutex_t * lock)
{
    pthread_mutex_unlock (lock);
}


#if 0
/*
 * To make the arc hash table huge. image the big table can contains 
 * all the 1M block in memory.
 */
uint64_t arc_hash_init ()
{
    uint64_t hsize = 1ULL << 12;
    uint64_t physmem = 0;
    physmem = getphymemsize ();
    while (hsize * (1 << 20) < physmem)
      {
	  hsize <<= 1;
      }
    return hsize;
}
#endif

void lfs_reopen ()
{
    close (lfs_n.fd);
#ifdef O_DIRECT_MODE
    lfs_n.fd = open (lfs_n.block_device, O_RDWR | O_DIRECT);
    SYS_OUT ("O_DIRECT MODE is open");
#else
    lfs_n.fd = open (lfs_n.block_device, O_RDWR);
#endif
    if (lfs_n.fd < 0)
      {
	  perror ("block device open failed");
	  exit (1);
      }
}

char *lfs_getshmptr (int shmid)
{
    char *ptr;
    if (shmid > 0)
	ptr = shmat (shmid, NULL, 0);
    return ptr;
}


int lfs_genflock (char *filename)
{
    int len;
    len = strlen (filename);
    char *ptr = malloc (14);
    memset (ptr, 0, 14);
    memset (lfs_n.instance.fname, 0, 20);

    sprintf (ptr, "/tmp/rfs%d.lock", dcache_hash (filename, len) % 1024);
    memcpy (lfs_n.instance.fname, ptr, strlen (ptr));
    if (access (lfs_n.instance.fname, F_OK) == 0)
      {
	  printf ("RFS filesystem instance is already running\n");
	  exit (0);
      }
    lfs_n.instance.fd = open (lfs_n.instance.fname, O_RDWR | O_CREAT, 0600);
    if (lfs_n.instance.fd < 0)
      {
	  perror ("create pidfile failed");
      }
    return LFS_OK;
}

void daemonize (const char *cmd)
{
    int i, fd0, fd1, fd2;
    pid_t pid;
    struct rlimit r1;
    struct sigaction sa;
    if (getrlimit (RLIMIT_NOFILE, &r1) < 0)
      {
	  perror ("can't get file limit\n");
      }

    if ((pid = fork ()) < 0)
	perror ("can't fork");
    else if (pid != 0)
	exit (0);
    setsid ();
    sa.sa_handler = SIG_IGN;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction (SIGHUP, &sa, NULL) < 0)
	perror ("can't ignore SIGHUP");

    if ((pid = fork ()) < 0)
	perror (" can't fork");
    else if (pid != 0)
	exit (0);
    if (chdir ("/") < 0)
	perror (" can't change directory to /");

    if (r1.rlim_max == RLIM_INFINITY)
	r1.rlim_max = 2048;

    lfs_genflock (cmd);
/* close stdin/stdout.
 */
    for (i = 0; i < r1.rlim_max; i++)
	close (i);
    fd0 = open ("/dev/null", O_RDWR);
    fd1 = dup (0);
    fd2 = dup (0);
}

void lfs_printf_debug (const char *fmt, ...)
{
    //  assert(0);
    va_list ap;
    char *ret;
    int err;
    va_start (ap, fmt);
    err = vasprintf (&ret, fmt, ap);
    va_end (ap);
//#ifdef _LFS_DEBUG
    printf ("%s", ret);
//#endif
}

void lfs_printf_err (const char *fmt, ...)
{

    va_list ap;
    char *ret;
    int err;
    va_start (ap, fmt);
    err = vasprintf (&ret, fmt, ap);
    va_end (ap);
    printf (ret);
}

void lfs_printf (const char *fmt, ...)
{

#ifdef _LFS_DEBUG
    va_list ap;
    char *ret;
    int err;
    va_start (ap, fmt);
    err = vasprintf (&ret, fmt, ap);
    va_end (ap);
    printf (ret);
#endif
}

int response_client_str (int clifd, char *ptr, int len)
{

    if (clifd < 0)
	lfs_printf ("client socket is invalid\n");
    if (write (clifd, ptr, len) < 0)
      {
	  perror ("response to client failed with -1");
	  return -1;

      }
    return LFS_SUCCESS;

}

int response_client (int clifd, int value)
{
    char num[5];
    int *ptr;
    ptr = (int *) num;
    *ptr = value;
    if (clifd < 0)
	lfs_printf ("client socket is invalid\n");
    if (write (clifd, num, 5 * sizeof (char)) < 0)
      {
	  perror ("response to client failed with -1");
	  return -1;

      }
    return LFS_SUCCESS;
}
