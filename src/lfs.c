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
