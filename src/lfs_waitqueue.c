#include <inttypes.h>
#include "lfs_waitqueue.h"
#include <memory.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>
#include "lfs_thread.h"
#include "arc.h"
#include "client/rfsio.h"
#include "lfs.h"
#include "lfs_sys.h"
extern lfs_info_t lfs_n;

static uint64_t pending_hash (uint64_t off, int id)
{

    uint64_t crc = 0;
    crc = (id << 5) ^ off;
    return crc;
}

inline void pending_hash_insert (pend_object_t * obj)
{
    unsigned long hash = pending_hash (obj->off, obj->id) & PQUEUE.ht_mask;
    lmutex_t *hash_lock;
    hash_lock = &PQUEUE.phash_mutexes[hash & (ARC_MUTEXES - 1)];
    mutex_enter (hash_lock, __func__, __LINE__);
    __arc_list_prepend (&obj->hash, &PQUEUE.bucket[hash]);
    lfs_printf ("&obj->hash next=%p\n", (&obj->hash)->next);
    mutex_exit (hash_lock, __func__);
}

void pend_hash_init ()
{
    uint64_t i, size;
    size = arc_hash_init ();
    assert (size > 0);
    lfs_n.pending_queue.size = size;
    lfs_n.pending_queue.ht_mask = size - 1;
    lfs_n.pending_queue.bucket = malloc (size * sizeof (struct __arc_list));
    for (i = 0; i < PQUEUE.size; i++)
      {
	  __arc_list_init (&PQUEUE.bucket[i]);
      }

}



pend_object_t *pobject_create (CQ_ITEM * item)
{
    pend_object_t *pobj;
    pobj = (pend_object_t *) malloc (sizeof (pend_object_t));
    if (pobj == NULL)
      {
	  lfs_printf ("pobj = null\n");
	  return NULL;
      }
    pobj->id = item->fid;
    pobj->clifd = item->clifd;
    pobj->off = item->offset;
    __arc_list_init (&pobj->hash);
    return pobj;
}


/*  
 * protocol : shmid
 */
inline int pending_hash_remove (struct object *obj)
{
    unsigned long hash = pending_hash (obj->offset, obj->id) & PQUEUE.ht_mask;
    struct __arc_list *list_head, *ptr, *iter;
    lmutex_t *hash_lock;
    uint64_t off, id;
    off = obj->offset;
    id = obj->id;
    hash_lock = &PQUEUE.phash_mutexes[hash & (ARC_MUTEXES - 1)];

    mutex_enter (hash_lock, __func__, __LINE__);
    if (obj == NULL)
      {
	  return -1;
      }
    list_head = &PQUEUE.bucket[hash];
    iter = list_head->next;
    for (ptr = iter->next; iter != list_head; ptr = ptr->next)
      {
	  char *bptr;
	  lfs_printf ("iter=%p\n", iter);
	  struct pend_object *_obj =
	      __arc_list_entry (iter, pend_object_t, hash);
	  if (_obj->id == id && _obj->off == off)
	    {
		__arc_list_remove (&_obj->hash);
		lfs_printf ("remove data in hash,line:%d\n", __LINE__);
		bptr = id2protocol (obj->obj_data->shmid);
		rfs_return_data (bptr, sizeof (int) + 1, _obj->clifd);
	    }
	  iter = ptr;
      }
    mutex_exit (hash_lock, __func__);
    return LFS_SUCCESS;
}
