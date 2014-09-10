#include <inttypes.h>
#include <memory.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>
#include "lfs_thread.h"
#include "arc.h"
#include "lfs.h"
//#include "client/rfsio.h"
#include "lfs_dcache.h"
#include "lfs_sys.h"
extern lfs_info_t lfs_n;

uint32_t dcache_hash (const char *str, int len)
{
    int32_t h = 0;
    int32_t i = 0;

    if (str == NULL)
      {
	  return 0;
      }
    for (i = 0; i < len; ++i)
      {
	  h += str[i];
	  h *= 7;
      }
    return (h | 0x80000000);
}

int dcache_hash_insert (uint32_t key, dcache_object_t * obj)
{
    unsigned long hash = key & DCACHE.ht_mask;
    struct __arc_list *list_head, *ptr, *iter;
    lmutex_t *hash_lock;
    int len = strlen (obj->fname);
    hash_lock = &DCACHE.phash_mutexes[hash & (ARC_MUTEXES - 1)];
    mutex_enter (hash_lock, __func__, __LINE__);
    if (obj == NULL)
      {
	  mutex_exit (hash_lock, __func__);
	  return -1;
      }
    list_head = &DCACHE.bucket[hash];
    iter = list_head->next;
    for (ptr = iter->next; iter != list_head; ptr = ptr->next)
      {
	  struct dcache_object *_obj =
	      __arc_list_entry (iter, dcache_object_t, hash);
	  if (obj->inode == _obj->inode
	      && strncmp (_obj->fname, obj->fname, len) == 0)
	    {
		assert (0);
		mutex_exit (hash_lock, __func__);
		return 1;	// existed in dcache,error
	    }
	  iter = ptr;
      }

    __arc_list_prepend (&obj->hash, &DCACHE.bucket[hash]);
    mutex_exit (hash_lock, __func__);
    return false;
}

void dcache_hash_init ()
{
    uint64_t i, size;
    size = arc_hash_init ();
    assert (size > 0);
    lfs_n.dcache.size = size;
    lfs_n.dcache.ht_mask = size - 1;
    lfs_n.dcache.bucket = malloc (size * sizeof (struct __arc_list));
    for (i = 0; i < DCACHE.size; i++)
      {
	  __arc_list_init (&DCACHE.bucket[i]);
      }
    for (i = 0; i < ARC_MUTEXES; i++)
      {
	  mutex_init (&DCACHE.phash_mutexes[i]);
      }
}

int dcache_remove (inode_t pinode, char *fname)
{
    int len = strlen (fname);
    uint32_t key = dcache_hash (fname, len);
    unsigned long hash = key & DCACHE.ht_mask;
    struct __arc_list *list_head, *ptr, *iter;
    lmutex_t *hash_lock;
    hash_lock = &DCACHE.phash_mutexes[hash & (ARC_MUTEXES - 1)];

    mutex_enter (hash_lock, __func__, __LINE__);

    list_head = &DCACHE.bucket[hash];
    iter = list_head->next;
    for (ptr = iter->next; iter != list_head; ptr = ptr->next)
      {
	  int len;
	  len = strlen (fname);
	  dcache_object_t *_obj =
	      __arc_list_entry (iter, dcache_object_t, hash);
	  if (strncmp (_obj->fname, fname, len) == 0
	      && pinode == _obj->pinode)
	    {
		__arc_list_remove (&_obj->hash);
		//bptr = id2protocol (obj->obj_data->shmid);
		//      rfs_return_data (bptr,sizeof(int)+1, _obj->clifd);
		lfs_printf ("Find ! remove this\n");
		mutex_exit (hash_lock, __func__);
		return true;
	    }
	  iter = ptr;
      }

    lfs_printf ("Not found this one remove this\n");
    mutex_exit (hash_lock, __func__);
    return false;

}


inode_t dcache_lookup (inode_t pinode, const char *fname)
{
    int len = strlen (fname);
    uint32_t key = dcache_hash (fname, len);
    unsigned long hash = key & DCACHE.ht_mask;
    struct __arc_list *list_head, *ptr, *iter;
    lmutex_t *hash_lock;
    hash_lock = &DCACHE.phash_mutexes[hash & (ARC_MUTEXES - 1)];

    mutex_enter (hash_lock, __func__, __LINE__);

    list_head = &DCACHE.bucket[hash];
    iter = list_head->next;
    for (ptr = iter->next; iter != list_head; ptr = ptr->next)
      {
	  int len;
	  len = strlen (fname);
	  dcache_object_t *_obj =
	      __arc_list_entry (iter, dcache_object_t, hash);
	  if (strncmp (_obj->fname, fname, len) == 0
	      && pinode == _obj->pinode)
	    {
		//bptr = id2protocol (obj->obj_data->shmid);
		//      rfs_return_data (bptr,sizeof(int)+1, _obj->clifd);
		mutex_exit (hash_lock, __func__);
		return _obj->inode;
	    }
	  iter = ptr;
      }
    mutex_exit (hash_lock, __func__);
    return 0;

}

int dcache_insert (inode_t pinode, const char *fname, inode_t inode)
{
    int len, res;
    uint32_t key;
    dcache_object_t *dobj;
    len = strlen (fname);
    dobj = malloc (sizeof (dcache_object_t));
    dobj->pinode = pinode;
    if (len > 0)
	dobj->fname = malloc (len + 1);
    else
	return -1;
    strncpy (dobj->fname, fname, strlen (fname));
    dobj->inode = inode;
    key = dcache_hash (fname, len);
    res = dcache_hash_insert (key, dobj);
    if (res == 1)
      {
	  lfs_printf ("the entry %s is existed in dcache\n", fname);
	  return -1;
      }
    else
      {
	  return 0;
      }
}
