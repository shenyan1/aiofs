#include<inttypes.h>
#include<stdio.h>
#include"lfs_define.h"
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<sys/time.h>
#include<stdarg.h>
#include<signal.h>
#include"lfs.h"
#include"lfs_sys.h"
#include<sys/resource.h>
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

int lfs_log (int fd, char *str)
{
    if (fd < 0)
	return -1;
    else
	write (fd, str, sizeof (str));
    return LFS_OK;
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


int lfs_trylock_fd (int fd)
{
    struct flock fl;
    memset (&fl, 0, sizeof (struct flock));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    if (fcntl (fd, F_SETLK, &fl) == -1)
	return -1;
    return LFS_OK;
}


int lfs_unlock_fd (int fd)
{
    struct flock fl;
    memset (&fl, 0, sizeof (struct flock));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    if (fcntl (fd, F_SETLK, &fl) == -1)
	return -1;
    return LFS_OK;
}
