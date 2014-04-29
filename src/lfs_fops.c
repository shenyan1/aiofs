#define _XOPEN_SOURCE 500
#include<stdio.h>
#include<unistd.h>
#include"lfs.h"
#include"arc.h"
#include"sarc.h"
#include<time.h>
#include<assert.h>
#include<stdlib.h>
#include"lfs_err.h"
#include"lfs_ops.h"
#include"lfs_thread.h"
#include"config.h"
#include"lfs_cache.h"

extern lfs_info_t lfs_n;
int
file_create (int size)
{
    int i;
    char filename[8] = { "LFS_T" };
    uint64_t tmp_off;
    int fsize_phy, is_free_phy;
    if (size > MAX_FSIZE)
	return -LFS_EBIG;
    if (size < AVG_FSIZE / 2)
      {
	  printf ("the buffer allocate less than 200M\n");
      }

    for (i = 0; i < lfs_n.max_files; i++)
      {
	  if (lfs_n.f_table[i].is_free == LFS_FREE)
	    {
		fsize_phy = lfs_n.f_table[i].fsize = size;
		is_free_phy = lfs_n.f_table[i].is_free = LFS_NFREE;
		tmp_off = getlocalp (i) - 16;

		printf ("id =%d is available\n,tmpoff=%" PRIu64 "", i,
			tmp_off);
		pwrite (lfs_n.fd, filename, sizeof (uint64_t), tmp_off);
		tmp_off += sizeof (uint64_t);
		pwrite (lfs_n.fd, &fsize_phy, sizeof (uint32_t), tmp_off);
		tmp_off += sizeof (uint32_t);
		pwrite (lfs_n.fd, &is_free_phy, sizeof (uint32_t), tmp_off);
		return i;
	    }
      }
    return -LFS_ENOSPC;
}

#define FNAMESIZE sizeof(uint64_t)
#define FILESIZE  sizeof(uint32_t)
#define FFLAG     sizeof(uint32_t)
#define BLKPTRSIZE sizeof(uint64_t)
#define TOTALSIZE (FNAMESIZE+FILESIZE+FFLAG+57*BLKPTRSIZE)
int
getfilemetadata (int id)
{
    uint64_t off;
    if (id < 0)
	return -LFS_INVALID;
    off = LFS_FILE_ENTRY;
    off += id * (FNAMESIZE + FILESIZE + FFLAG + 57 * BLKPTRSIZE);
    return off;
}

int
getfilesize (int id)
{
    uint64_t offset;
    int fsize;
    offset = getfilemetadata (id);
    if (offset == -LFS_INVALID)
      {
	  printf ("file io error\n");
	  return -1;
      }
    pread (lfs_n.fd, &fsize, sizeof (uint32_t), offset + FNAMESIZE);
    printf ("file size=%d\n", fsize);
    return fsize;
}

int
file_remove (int id)
{

    uint64_t off, size;
    char buffer[TOTALSIZE + 1];
    off = getfilemetadata (id);

    if (id > lfs_n.max_files)
	return -LFS_INVALID;
    if (off < 0 || lfs_n.f_table[id].is_free == LFS_FREE)
      {
#ifdef LFS_DEBUG
	  printf ("file_remove failed: id=%d is freed\n", id);
#endif
	  return -LFS_INVALID;
      }
    memset (buffer, 0, TOTALSIZE);
    pwrite (lfs_n.fd, buffer, TOTALSIZE, off);
    printf ("file removed\n");
    return 0;
}

inline uint64_t
getdiskpos (uint64_t offset)
{
    uint64_t off;
    off = offset / LFS_BLKSIZE;
    off = off * LFS_BLKSIZE;

    assert (off <= offset);
    return off;
}

inline uint64_t
getdiskrpos (uint64_t offset)
{

    if (offset % LFS_BLKSIZE == 0)
      {
	  return getdiskpos (offset);
      }
    return getdiskpos (offset) + (1 << 20);

}

/* LFS METHOD to write a file
 * param:int  id
 * param:char *buffer
 * param:int  size
 * param:uint64_t offset
 * return 0 means OK
 */
int
file_write (int id, char *buffer, uint64_t size, uint64_t offset)
{
    int ret;
    file_entry_t entry;
    entry = lfs_n.f_table[id];
    /* compute the blockid  in file id,offset / 1M;
     */
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  printf ("going to write a free dnode,id=%d\n", id);
	  return -LFS_EPM;
      }
    if (size > AVG_FSIZE)
      {
	  printf ("it will write larger than 200M\n");
	  return true;
      }
    if (offset + size > entry.fsize)
      {
	  printf ("write err,id=%d,write beyond filesize,filesize=%d,write %"
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
	  printf ("pwrite error\n");
	  return true;
      }
    return false;

}

int
file_open (int id, int flag)
{
    if (lfs_n.fd == -1)
      {
	  printf ("the fs is crashed\n");
	  return -LFS_EOPEN;
      }
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  printf ("the file %d is freed when open\n", id);
	  return -LFS_EOPEN;
      }
    return LFS_SUCCESS;
}

/* LFS METHOD to read a file
 * param:int   id
 * param:char *buffer
 * param:int   size
 * param:uint64_t offset
 * +++++++++++++++++++++++++++++++++++++++++++++++
 * |l_off       offset     |(r_off)                      |
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
int
file_read (int id, char *buffer, uint32_t size, uint64_t offset)
{

    uint64_t l_off, r_off;
    int nblks;
    int i, bufoff, tocpy;
    struct object *obj;
#ifndef USE_SARC
    struct __arc_object *entry;
#else
    struct __sarc_object *entry;
#endif
    l_off = getdiskpos (offset);
    r_off = getdiskpos (offset + size);
    if (buffer == NULL)
      {
	  printf ("the buffer shouldn't be NULL\n");
	  return -1;
      }
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  printf ("read refused id=%d isfree=%d\n ", id,
		  lfs_n.f_table[id].is_free);
	  return -LFS_EPM;
      }
    if (l_off == r_off)
      {
	  /* read from arc,by 1 MetaBytes.
	   */
#ifdef LFS_DEBUG
	  printf ("going to arc_lookup,id=%d,off=%" PRIu64 "\n", id, l_off);
#endif
#ifdef USE_SARC

	  entry = __sarc_lookup (lfs_n.sarc_cache, id, l_off);
#else
	  entry = __arc_lookup (lfs_n.arc_cache, id, l_off);
#endif
	  obj = __arc_list_entry (entry, struct object, entry);
	  memcpy (buffer, obj->data + (offset - l_off), size);
	  return 0;
      }
    /* the block is across only 1 block.
     */
    nblks = (getdiskrpos (offset + size) - getdiskpos (offset)) >> 20;
    bufoff = offset - l_off;
    tocpy = LFS_BLKSIZE - offset + bufoff;
    for (i = 0; i < nblks; i++)
      {
#ifndef USE_SARC
	  entry = __arc_lookup (lfs_n.arc_cache, id, l_off);
#else

	  entry = __sarc_lookup (lfs_n.arc_cache, id, l_off);
#endif
	  obj = __arc_list_entry (entry, struct object, entry);
	  bufoff = offset - obj->offset;
	  tocpy = (int) MIN (LFS_BLKSIZE - bufoff, size);
	  memcpy (buffer, obj->data + bufoff, tocpy);
	  offset += tocpy;
	  size -= tocpy;
	  buffer = buffer + tocpy;
	  l_off += LFS_BLKSIZE;
      }

    return 0;
}

extern struct __sarc_ops sarc_ops;
void
lfs_arc_init (uint64_t arc_size)
{
#ifndef USE_SARC
    lfs_n.arc_cache = __arc_create (&arc_ops, arc_size);
#else
    lfs_n.sarc_cache = __sarc_create (&sarc_ops, arc_size);
#endif
    if (lfs_n.arc_cache == NULL)
      {
	  printf ("lfs arc create failed\n");
	  exit (1);
      }

}
