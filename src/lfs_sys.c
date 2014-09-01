#include<inttypes.h>
#include<stdio.h>
#include"lfs_define.h"
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<sys/time.h>
#include<stdarg.h>
#include"lfs.h"
#include"lfs_sys.h"
uint64_t getlocalp (uint64_t id)
{
    uint64_t lp;
    assert (id >= 0);
    lp = LFS_FILE_ENTRY;
    lp += id * METASIZE_PERFILE;
    return lp;
}

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

uint64_t getphymemsize ()
{
    return sysconf (_SC_PHYS_PAGES) * sysconf (_SC_PAGESIZE);
}

int buf2id (char *ptr)
{
    int id;
    if (ptr == NULL)
	return 0;
    id = *(int *) ptr;
    return id;
}

u64 lfsgetblk (lfs_info_t * plfs_n, inode_t inode, u64 offset)
{
    int nblks;
    u64 blkptr = 0, rel_blkptr = 0;
    if (inode < 0 || inode > plfs_n->max_files)
	return 0;
    assert (plfs_n->f_table[inode].is_free == LFS_NFREE);
    if (offset < AVG_FSIZE)
	blkptr = plfs_n->f_table[inode].meta_table[0] + offset;
    else
      {
	  rel_blkptr = (offset - AVG_FSIZE) % LFS_BLKSIZE;
	  nblks = (offset - AVG_FSIZE) / LFS_BLKSIZE;
	  blkptr = plfs_n->f_table[inode].meta_table[1 + nblks] + rel_blkptr;
      }

    return blkptr;
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
    printf (ret);
//#endif
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