#include"lfs.h"

/* To get the dnode's metadata's location
 */
uint64_t
getlocalp (uint64_t id)
{
    uint64_t lp;
    lp = LFS_FILE_ENTRY;
    lp += 16;
    lp += id * (16 + FILE_ENTRYS * sizeof (uint64_t));
    return lp;
}

uint64_t
arc_hash (uint64_t id, uint64_t offset)
{

    uint64_t crc = -1;

    crc ^= (id >> 8) ^ offset;
    return crc;
}

uint64_t
getphymemsize ()
{
    return sysconf (_SC_PHYS_PAGES) * sysconf (_SC_PAGESIZE);
}

void
lfs_mutex (pthread_mutex_t * lock)
{
    pthread_mutex_lock (lock);
}

void
lfs_unmutex (pthread_mutex_t * lock)
{
    pthread_mutex_unlock (lock);
}

#if 0
/*
 * To make the arc hash table huge. image the big table can contains 
 * all the 1M block in memory.
 */
uint64_t
arc_hash_init ()
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
