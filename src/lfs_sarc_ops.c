#define _XOPEN_SOURCE 500
#include<stdio.h>
#include"config.h"
#include<unistd.h>
#include"lfs.h"
#include"arc.h"
#include"sarc.h"
#include<time.h>
#include<assert.h>
#include<stdlib.h>
#include"lfs_ops.h"
#include"lfs_thread.h"
#include"lfs_cache.h"
/*
 */
#ifdef USE_SARC
extern lfs_info_t lfs_n;
/* LFS METHOD to create a file
 * param: int size
 */
//      arc_cache =  cache_create("arc_cache", 1<<20, sizeof(char*),NULL, NULL);

/**
* Here are the operations implemented
*/
uint64_t __op_hash (uint64_t key)
{
    return key;
}

struct object *sarc_getobj (struct __sarc_object *e)
{
    return __arc_list_entry (e, struct object, entry);
}

static int
__op_compare (struct __sarc_object *e, uint64_t id, uint64_t offset)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);
    return obj->id == id && obj->offset == offset;
}

static struct __sarc_object *__op_create (uint64_t id, uint64_t offset)
{
    struct object *obj = cache_alloc (lfs_n.lfs_obj_cache);
    obj->data = cache_alloc (lfs_n.lfs_cache);
#ifdef LFS_DEBUG
    lfs_printf ("\n1:cache_alloc obj=%p,id=%" PRIu64 ",offset=%" PRIu64 "",
		&obj->entry, id, offset);
#endif
    if (offset < 0 || id < 0)
      {
	  lfs_printf ("offset =%" PRIu64 ",id=%" PRIu64
		      ",when create arc element\n", offset, id);
	  exit (1);
	  return NULL;
      }
    assert (id != 0);
    obj->offset = offset;
    obj->id = id;
/*
 * At this time we read from our arc buffer. if the buffer isn't in arc,we call this,so read from disk now.
 */
    //read from disk to obj->data
    __sarc_object_init (&obj->entry, offset, id, 1 << 20);
    return &obj->entry;
}

/* Prefetch operations that SARC ops' private method.
 */
static inline int
__op_prefetch_disk (uint64_t id, uint64_t offset, struct __sarc_object *e)
{
    int i;
    struct object *obj = sarc_getobj (e);
    assert (e->pcount == lfs_n.arc_cache->seqThreshold);
    pread (lfs_n.fd, obj->data, 1 << 20,
	   offset + lfs_n.f_table[id].meta_table[0]);
    return 1;
}

/* SARC maybe prefetch data here.
 */
static inline int
__op_fetch_disk (uint64_t id, uint64_t offset, struct __sarc_object *e)
{
    int i;
    struct object *obj = sarc_getobj (e);
    assert (e->pcount <= lfs_n.sarc_cache->seqThreshold);
    if (e->pcount != lfs_n.arc_cache->seqThreshold)
      {
	  pread (lfs_n.fd, obj->data, 1 << 20,
		 offset + lfs_n.f_table[id].meta_table[0]);
	  return 1;
      }
    else
      {
	  for (i = 1; i <= PREFETCH_LENGTH; i++)
	    {
		readandmru (id, offset + i * LFS_BLKSIZE);
	    }
      }
    return 1;
}

static int __op_fetch (struct __sarc_object *e)
{
    int ret;
    uint64_t offset, id;
    struct object *obj = __arc_list_entry (e, struct object, entry);
    obj->data = cache_alloc (lfs_n.lfs_cache);

    offset = obj->offset;
    id = obj->id;
#ifdef LFS_DEBUG
    lfs_printf ("\n2:cache_alloc obj=%p,id=%" PRIu64 ",offset=%" PRIu64 "",
		&obj->entry, id, offset);
#endif
    ret =
	pread (lfs_n.fd, obj->data, 1 << 20,
	       offset + lfs_n.f_table[id].meta_table[0]);
    if (ret != 1 << 20)
	return 1;

    return 0;
}

static void __op_evict (struct __sarc_object *e)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);
    cache_free (lfs_n.lfs_cache, obj->data);
}

static void __op_destroy (struct __sarc_object *e)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);
//              free(obj->data);
    mutex_destroy (&e->obj_lock);
//              lfs_printf("obj's offset=%"PRIu64"id=%"PRIu64"",obj->offset,obj->id);
    cv_destroy (&e->cv);
    cache_free (lfs_n.lfs_cache, obj->data);
    // free(obj);
    cache_free (lfs_n.lfs_obj_cache, obj);
}

struct __sarc_ops sarc_ops = {
    .hash = __op_hash,
    .cmp = __op_compare,
    .create = __op_create,
    .fetch = __op_fetch,
    .evict = __op_evict,
    .fetch_from_disk = __op_fetch_disk,
    .prefetch_from_disk = __op_prefetch_disk,
    .destroy = __op_destroy
};
#endif

#if 0
/* LFS METHOD to read a file
 * param:int   id
 * param:char *buffer
 * param:int   size
 * param:uint64_t offset
 * +++++++++++++++++++++++++++++++++++++++++++++++
 * |l_off       offset     |(r_off) 			 |
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
int sarc_file_read (int id, char *buffer, uint32_t size, uint64_t offset)
{

    uint64_t l_off, r_off;
    int nblks;
    int i, bufoff, tocpy;
    struct object *obj;
    struct __sarc_object *entry;
    l_off = getdiskpos (offset);
    r_off = getdiskpos (offset + size);
    if (buffer == NULL)
      {
	  lfs_printf ("the buffer shouldn't be NULL\n");
	  return -1;
      }
    if (lfs_n.f_table[id].is_free == LFS_FREE)
      {
	  lfs_printf ("read refused id=%d\n", id);
	  return -LFS_EPM;
      }
    if (l_off == r_off)
      {
	  /* read from arc,by 1 MetaBytes.
	   */
#ifdef LFS_DEBUG
	  lfs_printf ("going to arc_lookup,id=%d,off=%" PRIu64 "\n", id,
		      l_off);
#endif
	  entry = __sarc_lookup (lfs_n.arc_cache, id, l_off);
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
	  entry = __sarc_lookup (lfs_n.arc_cache, id, l_off);
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

#endif
