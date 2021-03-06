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
#include"lfs_cache.h"
#include"lfs_sys.h"
/*
 */
extern lfs_info_t lfs_n;
/* LFS METHOD to create a file
 * param: int size
 */
//      arc_cache =  cache_create("arc_cache", 1<<20, sizeof(char*),NULL, NULL);
struct object *getobj (struct __arc_object *e)
{
    return __arc_list_entry (e, struct object, entry);
}

/**
* Here are the operations implemented
*/
#ifndef USE_SARC
uint64_t __op_hash (uint64_t key)
{
    return key;
}

static int __op_compare (struct __arc_object *e, uint64_t id, uint64_t offset)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);
    return obj->id == id && obj->offset == offset;
}

static struct __arc_object *__op_create (uint64_t id, uint64_t offset)
{
    struct object *obj = cache_alloc (lfs_n.lfs_obj_cache);

    obj->obj_data = cache_alloc_shm (lfs_n.lfs_cache);
#ifdef LFS_DEBUG
    lfs_printf ("\n1:cache_alloc obj=%p,id=%" PRIu64 ",offset=%" PRIu64 "",
		&obj->entry, id, offset);
#endif
    assert (obj->obj_data->data);
    if (offset < 0 || id < 0)
      {
	  lfs_printf ("offset =%" PRIu64 ",id=%" PRIu64
		      ",when create arc element\n", offset, id);
	  exit (1);
	  return NULL;
      }
//    assert (id != 0);
    obj->offset = offset;
    obj->id = id;
/*
 * At this time we read from our arc buffer. if the buffer isn't in arc,we call this,so read from disk now.
 */
    //read from disk to obj->data
    __arc_object_init (&obj->entry, 1 << 20);
    return &obj->entry;
}

static inline int
__op_fetch_disk (uint64_t id, uint64_t offset, struct __arc_object *_obj)
{

    CQ_ITEM *item = cqi_new ();
    item->fops = READ_COMMAND;
    item->offset = offset;
    item->fid = id;
    item->obj = _obj;
//    lfs_printf("readstate =%d",_obj->read_state);
    assert (_obj->read_state == READ_STATE);
/*    
 * cq is aio queue.
 */
    cq_push (RFS_AIOQ, item);
    return 1;
}

static int __op_fetch (struct __arc_object *e)
{
    uint64_t offset, id;
    struct object *obj = __arc_list_entry (e, struct object, entry);
    obj->obj_data = cache_alloc_shm (lfs_n.lfs_cache);

    offset = obj->offset;
    id = obj->id;
#ifdef LFS_DEBUG
    lfs_printf ("\n2:cache_alloc obj=%p,id=%" PRIu64 ",offset=%" PRIu64 "",
		&obj->entry, id, offset);
#endif
    CQ_ITEM *item = cqi_new ();
    item->fops = READ_COMMAND;
    item->offset = offset;
    item->fid = id;
    item->obj = e;
    lfs_printf (" 2ci fetch begin id=%d\n", id);
    if (id < 0 || id > lfs_n.max_files)
	return -1;
    cq_push (RFS_AIOQ, item);
    return 0;
}

#if 0
static int __op_fetch (struct __arc_object *e)
{
    int ret;
    uint64_t blkptr, offset, id;
    struct object *obj = __arc_list_entry (e, struct object, entry);
    obj->obj_data = cache_alloc (lfs_n.lfs_cache);

    offset = obj->offset;
    id = obj->id;
#ifdef LFS_DEBUG
    lfs_printf ("\n2:cache_alloc obj=%p,id=%" PRIu64 ",offset=%" PRIu64 "",
		&obj->entry, id, offset);
#endif
// unfinished, when file size > 200MB
    blkptr = lfsgetblk (&lfs_n, id, offset);
    if (blkptr == NULL)
      {
	  lfs_printf ("invalid arguments in arc when miss\n");
	  ret = -1;
      }
    else
      {
	  ret =
	      pread (lfs_n.fd, obj->obj_data->data, 1 << 20,
		     offset + lfs_n.f_table[id].meta_table[0]);
	  if (ret != 1 << 20)
	      ret = -1;
      }
    return ret;
}
#endif
static void __op_evict (struct __arc_object *e)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);
#ifndef CONFIG_SHMEM
    cache_free (lfs_n.lfs_cache, obj->obj_data);
    e->read_state = 10;
#else
    e->read_state = 10;
    cache_free_shm (lfs_n.lfs_cache, obj->obj_data);
#endif
    lfs_printf ("evict happend!\n");
}

static void __op_destroy (struct __arc_object *e)
{
    struct object *obj = __arc_list_entry (e, struct object, entry);

    mutex_destroy (&e->obj_lock);
//              lfs_printf("obj's offset=%"PRIu64"id=%"PRIu64"",obj->offset,obj->id);
    cv_destroy (&e->cv);
    if (e->state != &lfs_n.arc_cache->mrug
	&& e->state != &lfs_n.arc_cache->mfug)
      {
	  //lfs_printf("evict to shm cache\n");
	  cache_free_shm (lfs_n.lfs_cache, obj->obj_data);
      }
    e->read_state = 0;
    cache_free (lfs_n.lfs_obj_cache, obj);
}

struct __arc_ops arc_ops = {
    .hash = __op_hash,
    .cmp = __op_compare,
    .create = __op_create,
    .fetch = __op_fetch,
    .evict = __op_evict,
    .fetch_from_disk = __op_fetch_disk,
    .destroy = __op_destroy
};
#endif
