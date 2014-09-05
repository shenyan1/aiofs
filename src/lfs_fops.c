#define _XOPEN_SOURCE 500
#include<stdio.h>
#include<unistd.h>
#include"lfs.h"
#include"arc.h"
#include"sarc.h"
#include<time.h>
#include<assert.h>
#include<stdlib.h>
#include"lfs_ops.h"
#include"lfs_thread.h"
#include"config.h"
#include"lfs_sys.h"
#include"lfs_cache.h"

#define FILESIZE  sizeof(uint32_t)
#define BLKPTRSIZE sizeof(uint64_t)

extern lfs_info_t lfs_n;

int check_file_valid (inode_t inode)
{
    return true;
}

int _finode_syncfree (int i, uint64_t off)
{
    uint8_t isfree = LFS_NFREE;
    lfs_printf ("##################isfree offset=%d", off + 4);
    _lfs_pwrite (lfs_n.fd, &isfree, 1, off + 4);
    //  fsync (lfs_n.fd);
    return 0;
}

int _finode_sync (int i, uint64_t off)
{
    _lfs_pwrite (lfs_n.fd, &lfs_n.f_table[i], sizeof (file_entry_t), off);
    fsync (lfs_n.fd);
    return 0;
}

inode_t finode_fallocate (inode_t inode, uint64_t size)
{
    uint64_t tmp_off;
    int i, res_size, nblks, j;
    i = inode;
    i = inode - LFS_FINODE_START;
    if (i >= lfs_n.max_files || lfs_n.f_table[i].is_free == LFS_FREE)
      {
	  lfs_printf ("fallocate failed invalid inode !\n");
	  return LFS_FAILED;
      }
    tmp_off = getlocalp (i);
    tmp_off += 3 * 8;
    _finode_syncfree (i, tmp_off);
    lfs_n.f_table[i].is_free = LFS_NFREE;
    if (size <= AVG_FSIZE)
	lfs_n.f_table[i].fsize = size;
    else if (size > AVG_FSIZE && size <= MAX_FSIZE)
      {
	  res_size = size - AVG_FSIZE;
	  nblks = res_size / LFS_BLKSIZE;
	  if (res_size % LFS_BLKSIZE != 0)
	      nblks++;
	  for (j = 1; j <= nblks; j++)
	      lfs_n.f_table[i].meta_table[j - 1] = Malloc_Freemap ();
	  lfs_n.f_table[i].fsize = size;
      }
    lfs_printf ("fallocate size=%d", lfs_n.f_table[i].fsize);
    _finode_sync (i, tmp_off);
    return LFS_OK;
}

inode_t finode_alloc ()
{
    uint64_t tmp_off;
    int i;
    for (i = 0; i < lfs_n.max_files; i++)
      {

	  if (lfs_n.f_table[i].is_free == LFS_FREE)
	    {
		tmp_off = getlocalp (i);
		tmp_off += 3 * 8;
		_finode_syncfree (i, tmp_off);
		lfs_n.f_table[i].is_free = LFS_NFREE;

		_finode_sync (i, tmp_off);
		lfs_printf ("##########finode_alloc: inode=%d\n", i + 1);
		return i + 1 + LFS_FINODE_START;
	    }
	  else
	    {
//              printf("file is not free:%d\n",lfs_n.f_table[i].is_free);
	    }
      }

    lfs_printf ("###############null finode_alloc\n");
    return 0;
}

#if 0
int getfilesize (int id)
{
    uint64_t offset;
    int fsize;
    offset = getlocalp (id);
    if (offset == -LFS_INVALID)
      {
	  lfs_printf ("file io error\n");
	  return -1;
      }
    pread (lfs_n.fd, &fsize, sizeof (uint32_t), offset + FNAMESIZE);
    lfs_printf ("file size=%d\n", fsize);
    return fsize;
}
#endif
/* internal remove a file syncly.
 * return 0 remove failed.
 *        1 remove file successfully.
 */
int _removeFile (inode_t id)
{

    u64 size;
    u64 off;
    int nblks = 0, i;
    char buffer[METASIZE_PERFILE];
    off = getlocalp (id);
    id = id - LFS_FINODE_START;
    if (id > lfs_n.max_files)
	return LFS_FAILED;
    if (off < 0 || id <= 0 || id > lfs_n.max_files
	|| lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  lfs_printf ("file_remove failed: id=%d is freed\n", id);
	  return 0;
      }
    size = lfs_n.f_table[id].fsize;
    memset (buffer, 0, METASIZE_PERFILE);
    if (size > AVG_FSIZE)
	nblks = (size - AVG_FSIZE) / LFS_BLKSIZE + 1;
    for (i = 0; i < nblks; i++)
      {
	  lfs_printf ("free small blkptr\n");
	  Free_Freemap (lfs_n.f_table[id].meta_table[1 + i]);
      }
    pwrite (lfs_n.fd, buffer, 8 * 3 + 8 + 5, off);
    off += 8 * 3 + 8 + 5;
    off += 8;
    pwrite (lfs_n.fd, buffer, 56 * 8, off);
    lfs_printf ("file removed\n");
    lfs_n.f_table[id].is_free = LFS_FREE;
    return 1;
}

inline uint64_t _getdiskpos (uint64_t offset)
{
    uint64_t off;
    off = offset / LFS_BLKSIZE;
    off = off * LFS_BLKSIZE;

    assert (off <= offset);
    return off;
}

inline uint64_t getdiskrpos (uint64_t offset)
{

    if (offset % LFS_BLKSIZE == 0)
      {
	  return P2ALIGN (offset, LFS_BLKSIZE);
      }
    return P2ALIGN (offset, LFS_BLKSIZE) + (1 << 20);

}

#if 0
int file_write (int id, char *buffer, uint64_t size, uint64_t offset)
{
    int ret;
    file_entry_t entry;
    entry = lfs_n.f_table[id];
    /* compute the blockid  in file id,offset / 1M;
     */
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  lfs_printf ("going to write a free dnode,id=%d\n", id);
	  return -LFS_EPM;
      }
    if (size > AVG_FSIZE)
      {
	  lfs_printf ("it will write larger than 200M\n");
	  return true;
      }
    if (offset + size > entry.fsize)
      {
	  lfs_printf
	      ("write err,id=%d,write beyond filesize,filesize=%d,write %"
	       PRIu64 "\n", id, entry.fsize, offset + size);
	  return -LFS_EBOUND;
      }
    /* in the write method, so we can just write the buffer with the offset and size.
     */

    ret =
	pwrite (lfs_n.fd, buffer, size,
		offset + lfs_n.f_table[id].meta_table[0]);
    if (ret == -1)
      {
	  lfs_printf ("pwrite error\n");
	  return true;
      }
    return false;

}
#endif

extern struct __sarc_ops sarc_ops;
void lfs_arc_init (uint64_t arc_size)
{
#ifndef USE_SARC
    lfs_n.arc_cache = __arc_create (&arc_ops, arc_size);
#else
    lfs_n.sarc_cache = __sarc_create (&sarc_ops, arc_size);
#endif
    if (lfs_n.arc_cache == NULL)
      {
	  lfs_printf ("lfs arc create failed\n");
	  exit (1);
      }

}
