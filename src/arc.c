#include "arc.h"
#include "lfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "lfs_thread.h"
#include "lfs_ops.h"
#include"lfs_sys.h"
#include "lfs_cache.h"
extern lfs_info_t lfs_n;
uint64_t lfs_arc_hash (uint64_t id, uint64_t offset)
{

    uint64_t crc = 0;

    crc = (id << 5) ^ offset;
    return crc;
}

inline void
__arc_list_prepend (struct __arc_list *head, struct __arc_list *list)
{
    __arc_list_insert (head, list, list->next);
}

inline void __arc_list_remove (struct __arc_list *head)
{
    __arc_list_splice (head->prev, head->next);
    head->next = head->prev = NULL;
}

inline void
__arc_list_insert (struct __arc_list *list, struct __arc_list *prev,
		   struct __arc_list *next)
{
    next->prev = list;
    list->next = next;
    list->prev = prev;
    prev->next = list;
}



/*
 * To make the arc hash table huge. image the big table can contains 
 * all the 1M block in memory.
 */
void print_state (struct __arc *cache)
{
    lfs_printf ("mru:%p,mfu:%p,mrug=%p,mfug=%p\n", &cache->mru, &cache->mfu,
		&cache->mrug, &cache->mfug);
}

int get_state (struct __arc_state *p)
{

    if (p == &lfs_n.arc_cache->mru)
	lfs_printf (":mru");
    else if (p == &lfs_n.arc_cache->mfu)
	lfs_printf (":mfu");
    else if (p == &lfs_n.arc_cache->mfug)
	lfs_printf (":mfug");
    else if (p == &lfs_n.arc_cache->mrug)
	lfs_printf (":mrug");
    else
	lfs_printf ("%p state", p);
    return 1;
}

void printsize (struct __arc *cache)
{
#ifdef LFS_DEBUG
    lfs_printf ("\ncache->c=%" PRIu64 ",cache->p=%" PRIu64 "\n", cache->c,
		cache->p);
    lfs_printf ("mru  size=%" PRIu64 "\n", cache->mru.size);
    lfs_printf ("mrug state size=%" PRIu64 "\n", cache->mrug.size);
    lfs_printf ("mfu state size=%" PRIu64 "\n", cache->mfu.size);
    lfs_printf ("mfug state size=%" PRIu64 "\n", cache->mfug.size);
#endif
}

void print_obj (struct __arc_object *obj, const char *str)
{
    struct object *objc;
    objc = getobj (obj);
    printf ("%s: obj=%p offset=%" PRIu64 ",read_state =%d,id=%" PRIu64
	    "\n", str, obj, objc->offset, obj->read_state, objc->id);
}

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

/* A simple hashtable with fixed bucket count. */
static void __arc_hash_init (struct __arc *cache)
{
    uint64_t i;
    cache->hash.size = arc_hash_init ();
    lfs_printf ("hash size = %" PRIu64 "\n", cache->hash.size);

    cache->hash.ht_mask = cache->hash.size - 1;
    cache->hash.bucket =
	malloc (cache->hash.size * sizeof (struct __arc_list));

    for (i = 0; i < cache->hash.size; ++i)
      {
	  __arc_list_init (&cache->hash.bucket[i]);
      }
}


static inline void
__arc_hash_insert (struct __arc *cache, uint64_t key,
		   struct __arc_object *obj, lmutex_t * hash_lock)
{
    unsigned long hash = cache->ops->hash (key) & cache->hash.ht_mask;
    struct __arc_list *iter;
    struct object *objc = getobj (obj);
    uint64_t id, offset;
    id = objc->id;
    offset = objc->offset;
    __arc_list_each (iter, &cache->hash.bucket[hash])
    {
	struct __arc_object *obj =
	    __arc_list_entry (iter, struct __arc_object, hash);
	if (cache->ops->cmp (obj, id, offset) == 1)
	  {
	      lfs_printf ("existed in hash when insert,line:%d\n", __LINE__);
	      mutex_exit (hash_lock, __func__);
	      exit (1);
	      return;
	  }
    }

    __arc_list_prepend (&obj->hash, &cache->hash.bucket[hash]);
    mutex_exit (hash_lock, __func__);
}

static struct __arc_object *__arc_hash_lookup (struct __arc *cache,
					       uint64_t id, uint64_t offset,
					       lmutex_t ** mem_lock)
{
    struct __arc_list *iter;
    uint64_t key;
    key = lfs_arc_hash (id, offset);
    unsigned long hash = cache->ops->hash (key) & cache->hash.ht_mask;
    lmutex_t *hash_lock = &cache->hash.hash_mutexes[hash];
    *mem_lock = hash_lock;
    mutex_enter (hash_lock, __func__, __LINE__);
    __arc_list_each (iter, &cache->hash.bucket[hash])
    {
	struct __arc_object *obj =
	    __arc_list_entry (iter, struct __arc_object, hash);
	if (cache->ops->cmp (obj, id, offset) == 1)
	  {
	      //      mutex_exit(hash_lock);
	      return obj;
	  }
    }

    return NULL;
}

static void __arc_hash_fini (struct __arc *cache)
{
    free (cache->hash.bucket);
}

/* Initialize a new object with this function. */
void __arc_object_init (struct __arc_object *obj, unsigned long size)
{

    mutex_init (&obj->obj_lock);
    obj->state = NULL;
    obj->size = size;
    obj->read_state = READ_STATE;
    assert (pthread_cond_init (&obj->cv, NULL) == 0);
    __arc_list_init (&obj->head);
    __arc_list_init (&obj->hash);
}

static inline unsigned long
gethash_lock (struct __arc *cache, struct __arc_object *_obj)
{
    struct object *obj = getobj (_obj);
    uint64_t id, offset, key;
    unsigned long hash;
    id = obj->id;
    offset = obj->offset;
    key = lfs_arc_hash (id, offset);
    hash = cache->ops->hash (key) & cache->hash.ht_mask;
    return hash;
}

/* Forward-declaration needed in __arc_move(). */
//static void __arc_balance(struct __arc *cache, unsigned long size);

static struct __arc_object *__arc_state_lru (struct __arc_state *state)
{
    struct __arc_list *head = state->head.prev;
    return __arc_list_entry (head, struct __arc_object, head);
}

void print_stateinfo (struct __arc_state *state1)
{
//   lfs_printf_debug("count=%d, state size=%"PRIu64",head=%p\n",state1->count,state1->size,state1->head);
}

static struct __arc_object *__arc_move_state (struct __arc *cache,
					      struct __arc_state *state1,
					      struct __arc_state *state2)
{
    struct __arc_object *obj;
    mutex_enter (&state1->state_lock, __func__, __LINE__);
    if (state1 == &lfs_n.arc_cache->mfu)
      {
	  print_stateinfo (state1);

      }
    obj = __arc_state_lru (state1);
    assert (obj->state == state1);
    state1->size -= obj->size;
    state1->count--;
    printsize (cache);
#ifdef LFS_DEBUG
    lfs_printf ("state have %lu objs", state1->count);
    print_obj (obj, "arc_move_state:");
#endif
    if (obj->state == NULL)
	exit (1);
#ifdef LFS_DEBUG
    //print_state(cache);
    //      lfs_printf("obj=%p,obj->state = %p\n",obj,obj->state);
    if (obj->state == &cache->mfu)
      {
	  lfs_printf ("remove from mfu,obj=%p,nums:%u\n", obj,
		      obj->state->count);
	  if (state == &cache->mfu)
	      lfs_printf
		  ("mfu to mfu,obj:prev=%llu,next=%llu,obj->head=%llu\n",
		   (unsigned long long) obj->head.prev,
		   (unsigned long long) obj->head.next,
		   (unsigned long long) &obj->head);
	  if (state == &cache->mfug)
	      lfs_printf ("move from mfu to mfug\n");
      }

#endif
    __arc_list_remove (&obj->head);
    mutex_exit (&obj->state->state_lock, __func__);

    if (state2 == NULL)
      {
	  lmutex_t *hash_lock;
	  unsigned long hash = gethash_lock (cache, obj);
	  assert (hash < ARC_MUTEXES);
	  hash_lock = &cache->hash.hash_mutexes[hash];

	  /* The object is being removed from the cache, destroy it. */
	  mutex_enter (hash_lock, __func__, __LINE__);
	  //lfs_printf ("move to null list");

	  // get_state(obj->state);
	  print_obj (obj, __func__);
//        lfs_printf("obj's read state=%d",obj->read_state);
	  cache->ops->destroy (obj);

	  __arc_list_remove (&obj->hash);
	  mutex_exit (hash_lock, __func__);
	  return NULL;
      }
    else
      {
	  if (state2 == &cache->mrug || state2 == &cache->mfug)
	    {
		/* The object is being moved to one of the ghost lists, evict
		 * the object from the cache. */
#ifdef LFS_DEBUG
		if (state2 == &cache->mrug)
		    lfs_printf ("evict:moving into the mrug ");
		else
		    lfs_printf ("evict:moving into the mfug ");
		lfs_printf ("arc_move_state:in cache_alloc evict");

#endif
		cache->ops->evict (obj);
	    }
	  else if (state1 != &cache->mru && state1 != &cache->mfu)
	    {
		/* The object is being moved from one of the ghost lists into
		 * the MRU or MFU list, fetch the object into the cache. */
		//__arc_balance(cache, obj->size);
		assert (state1 == &cache->mrug || state1 == &cache->mfug);
		lfs_printf ("arc_move state:in cache_alloc\n");
		if (cache->ops->fetch (obj))
		  {
		      /* If the fetch fails, put the object back to the list
		       * it was in before. */
		      assert (0);
		      mutex_enter (&state1->state_lock, __func__, __LINE__);
		      obj->state->size += obj->size;
		      obj->state->count++;
		      __arc_list_prepend (&obj->head, &obj->state->head);

		      mutex_exit (&state1->state_lock, __func__);
		      lfs_printf ("fetch failed in arc_move\n");
		      return NULL;
		  }
	    }
	  mutex_enter (&state2->state_lock, __func__, __LINE__);
	  __arc_list_prepend (&obj->head, &state2->head);
	  obj->state = state2;
	  obj->state->size += obj->size;
	  assert (state2 != &cache->mru);
	  assert (state2 != &cache->mfu);
	  obj->state->count++;
	  mutex_exit (&state2->state_lock, __func__);
      }

    return obj;
}


/* Move the object to the given state. If the state transition requires,
* fetch, evict or destroy the object. */
static struct __arc_object *__arc_move (struct __arc *cache,
					struct __arc_object *obj,
					struct __arc_state *state, int flag)
{
    if (obj->state)
      {
	  if (flag == OBJ_UNLOCK)
	      mutex_enter (&obj->state->state_lock, __func__, __LINE__);
	  obj->state->size -= obj->size;
	  //lfs_printf("obj's size=%"PRIu64",state's size=%"PRIu64"\n",obj->size,obj->state->size);
	  obj->state->count--;
#ifdef LFS_DEBUG
	  lfs_printf ("obj=%p,obj->state = %p\n", obj, obj->state);
	  //print_state(cache);
	  if (obj->state == &cache->mfu)
	    {
		lfs_printf ("remove from mfu,obj=%p,nums:%u\n", obj,
			    obj->state->count);
		if (state == &cache->mfu)
		    lfs_printf
			("mfu to mfu,obj:prev=%llu,next=%llu,obj->head=%llu\n",
			 (unsigned long long) obj->head.prev,
			 (unsigned long long) obj->head.next,
			 (unsigned long long) &obj->head);
		if (state == &cache->mfug)
		    lfs_printf ("move from mfu to mfug\n");
	    }

#endif
	  __arc_list_remove (&obj->head);
	  mutex_exit (&obj->state->state_lock, __func__);
      }

    if (state == NULL)
      {
	  lmutex_t *hash_lock;
	  unsigned long hash = gethash_lock (cache, obj);
	  assert (hash < ARC_MUTEXES);
	  hash_lock = &cache->hash.hash_mutexes[hash];

	  /* The object is being removed from the cache, destroy it. */
	  mutex_enter (hash_lock, __func__, __LINE__);
	  __arc_list_remove (&obj->hash);
	  cache->ops->destroy (obj);
	  mutex_exit (hash_lock, __func__);
	  return NULL;
      }
    else
      {
	  if (state == &cache->mrug || state == &cache->mfug)
	    {
		/* The object is being moved to one of the ghost lists, evict
		 * the object from the cache. */

#ifdef LFS_DEBUG
		if (state == &cache->mrug)
		    lfs_printf ("moving into the mrug ");
		else
		    lfs_printf ("moving into the mfug ");
#endif
		lfs_printf ("arc_move:in cache_alloc evict");
		cache->ops->evict (obj);
	    }
	  else if (obj->state != &cache->mru && obj->state != &cache->mfu
		   && flag != OBJ_INIT)
	    {
		/* The object is being moved from one of the ghost lists into
		 * the MRU or MFU list, fetch the object into the cache. */
		//__arc_balance(cache, obj->size);

		assert (obj->state != NULL);
		assert (obj->state == &cache->mrug
			|| obj->state == &cache->mfug);
		if (cache->ops->fetch (obj))
		  {
		      /* If the fetch fails, put the object back to the list
		       * it was in before. */
		      assert (0);
		      mutex_enter (&state->state_lock, __func__, __LINE__);
		      obj->state->size += obj->size;
		      obj->state->count++;
		      __arc_list_prepend (&obj->head, &obj->state->head);

		      mutex_exit (&state->state_lock, __func__);
#ifdef LFS_DEBUG
		      lfs_printf ("fetch failed in arc_move\n");
#endif
		      return NULL;
		  }
	    }
	  mutex_enter (&state->state_lock, __func__, __LINE__);
	  __arc_list_prepend (&obj->head, &state->head);
	  obj->state = state;
	  obj->state->size += obj->size;

	  obj->state->count++;
#ifdef LFS_DEBUG
	  if (state == &cache->mfu)
	      lfs_printf ("going to move to mfu obj=%p,nums=%lu\n", obj,
			  state->count);
#endif
	  if (state == &cache->mru)
	      ;			//lfs_printf("become mru in arc_move obj=%p\n ",obj);
	  mutex_exit (&state->state_lock, __func__);
      }

    return obj;
}

/* Return the LRU element from the given state. */
static void __arc_replace (struct __arc *cache, int ismfug)
{
    if ((cache->mru.size != 0 && cache->mru.size > cache->p)
	|| (ismfug != 0 && cache->mru.size == cache->p))
      {
	  __arc_move_state (cache, &cache->mru, &cache->mrug);
      }
    else
      {
	  __arc_move_state (cache, &cache->mfu, &cache->mfug);
      }
}

static void __arc_adjust (struct __arc *cache)
{
    printsize (cache);
    /* case A:
     */
    if (cache->mru.size + cache->mrug.size == cache->c)
      {
	  if (cache->mru.size < cache->c)
	    {
		struct __arc_object *obj = __arc_state_lru (&cache->mrug);
		assert (obj->state == &cache->mrug);
		__arc_move_state (cache, &cache->mrug, NULL);
		__arc_replace (cache, 0);
	    }
	  else
	    {
		assert (cache->mrug.size == 0);
		struct __arc_object *obj = __arc_state_lru (&cache->mru);
		assert (obj->state == &cache->mru);
		__arc_move_state (cache, &cache->mru, NULL);
	    }
      }
    /* case B: */
    if (cache->mru.size + cache->mrug.size < cache->c)
      {
	  if (cache->mru.size + cache->mrug.size + cache->mfu.size +
	      cache->mfug.size >= cache->c)
	    {
		if (cache->mru.size + cache->mfu.size + cache->mrug.size +
		    cache->mfug.size == 2 * cache->c)
		    __arc_move_state (cache, &cache->mfug, NULL);
		__arc_replace (cache, 0);

	    }
      }
}

/* Balance the lists so that we can fit an object with the given size into
 * the cache. */
#if 0
static void __arc_balance (struct __arc *cache, unsigned long size)
{
    //lfs_printf("going to balance in arc\n");
    /* First move objects from MRU/MFU to their respective ghost lists. */
    printsize (cache);
    while (cache->mru.size + cache->mfu.size + size > cache->c)
      {
	  if (cache->mru.size > cache->p)
	    {
		//      mutex_enter(&cache->mru.state_lock);
		struct __arc_object *obj = __arc_state_lru (&cache->mru);

		lfs_printf ("put the obj %p state =%p into mrug,%d\n", obj,
			    obj->state, (int) pthread_self ());
		assert (obj->state == &cache->mru);
		__arc_move (cache, obj, &cache->mrug, OBJ_UNLOCK);
		lfs_printf ("move finished obj->state =%p\n", obj->state);
	    }
	  else if (cache->mfu.size > 0)
	    {
		lfs_printf ("in while go to balance tid:%d\n",
			    (int) pthread_self ());
		printsize (cache);

		struct __arc_object *obj = __arc_state_lru (&cache->mfu);
		assert (&cache->mfu == obj->state);
		__arc_move (cache, obj, &cache->mfug, OBJ_UNLOCK);

	    }
	  else
	    {
		break;
	    }
      }
    /* Then start removing objects from the ghost lists. */
    while (cache->mrug.size + cache->mfug.size > cache->c)
      {
	  if (cache->mfug.size > cache->p)
	    {
		struct __arc_object *obj = __arc_state_lru (&cache->mfug);
		__arc_move (cache, obj, NULL, OBJ_UNLOCK);
	    }
	  else if (cache->mrug.size > 0)
	    {
		//      mutex_enter(&cache->mrug.state_lock);
		struct __arc_object *obj = __arc_state_lru (&cache->mrug);
		__arc_move (cache, obj, NULL, OBJ_UNLOCK);
	    }
	  else
	    {
		break;
	    }
      }
    //lfs_printf("end arc balance\n");
}
#endif

/* Create a new cache. */
struct __arc *__arc_create (struct __arc_ops *ops, uint64_t c)
{
    int i;
    struct __arc *cache = malloc (sizeof (struct __arc));
    memset (cache, 0, sizeof (struct __arc));

    cache->ops = ops;

    __arc_hash_init (cache);
    c -= c % LFS_BLKSIZE;

    cache->c = c;
    cache->p = c >> 1;

    cache->arc_stats.hits = 0;
    cache->arc_stats.total = 0;
    mutex_init (&cache->arc_stats.stat_lock);
    mutex_init (&cache->arc_lock);
    mutex_init (&cache->arc_balock);
    __arc_list_init (&cache->mrug.head);
    __arc_list_init (&cache->mru.head);
    __arc_list_init (&cache->mfu.head);
    __arc_list_init (&cache->mfug.head);
/*  the last hash_mutexes is reserved for list_remove
 */
    for (i = 0; i <= ARC_MUTEXES; i++)
      {
	  mutex_init (&cache->hash.hash_mutexes[i]);
      }
    cache->hash.ht_mask = ARC_MUTEXES - 1;

    mutex_init (&cache->mrug.state_lock);
    mutex_init (&cache->mru.state_lock);
    mutex_init (&cache->mfu.state_lock);
    mutex_init (&cache->mfug.state_lock);
    print_state (cache);
    return cache;
}

static inline void
arc_list_destroy (struct __arc_state *state, struct __arc *cache)
{

    struct __arc_object *obj;
    while (state->head.prev != state->head.next && state->head.prev != NULL)
      {
	  obj = __arc_state_lru (state);
	  __arc_list_remove (&obj->head);
	  cache->ops->destroy (obj);

      }
}

/* Destroy the given cache. Free all objects which remain in the cache. */
void __arc_destroy (struct __arc *cache)
{
    //   struct __arc_list *iter;
    int i;
    arc_list_destroy (&cache->mrug, cache);
    arc_list_destroy (&cache->mru, cache);
    arc_list_destroy (&cache->mfu, cache);
    arc_list_destroy (&cache->mfug, cache);

    for (i = 0; i <= ARC_MUTEXES; i++)
      {
	  mutex_destroy (&cache->hash.hash_mutexes[i]);
      }

    __arc_hash_fini (cache);
    mutex_destroy (&cache->arc_balock);
    mutex_destroy (&cache->arc_lock);
    mutex_destroy (&cache->arc_stats.stat_lock);
    mutex_destroy (&cache->mrug.state_lock);
    mutex_destroy (&cache->mru.state_lock);
    mutex_destroy (&cache->mfu.state_lock);
    mutex_destroy (&cache->mfug.state_lock);
    free (cache);
}


static inline void arc_stat_hit_update (struct __arc *cache)
{
    mutex_enter (&cache->arc_stats.stat_lock, __func__, __LINE__);
    cache->arc_stats.hits++;
    cache->arc_stats.total++;
    mutex_exit (&cache->arc_stats.stat_lock, __func__);
}

static inline void arc_stat_update (struct __arc *cache)
{

    mutex_enter (&cache->arc_stats.stat_lock, __func__, __LINE__);
    cache->arc_stats.total++;
    mutex_exit (&cache->arc_stats.stat_lock, __func__);
}

inline void arc_read_done (struct __arc_object *obj)
{
    uint64_t key;
    struct object *bobj = getobj (obj);
    key = lfs_arc_hash (bobj->id, bobj->offset);
    obj = __arc_move (lfs_n.arc_cache, obj, &lfs_n.arc_cache->mru, OBJ_INIT);
    assert (obj->state == &lfs_n.arc_cache->mru);

    mutex_enter (&obj->obj_lock, __func__, __LINE__);
    if (obj->read_state != READ_STATE)
	print_obj (obj, __func__);
    assert (obj->read_state == READ_STATE);
    obj->read_state = READ_FINISHED;

    mutex_exit (&obj->obj_lock, __func__);
/*notify other pending requests*/
    pending_hash_remove (bobj);
}

/* Lookup an object with the given key. */
struct __arc_object *__arc_lookup (struct __arc *cache, CQ_ITEM * item)
{
    uint64_t id, offset;
    pend_object_t *pobj;
#define IN_MFUG 1
    struct __arc_object *obj;
    lmutex_t *hash_lock;
    uint64_t key;
    id = item->fid;
    offset = item->offset;
    offset = item->offset = P2ALIGN (offset, LFS_BLKSIZE);

    key = lfs_arc_hash (id, offset);
#ifdef LFS_DEBUG

    pthread_t tid = pthread_self ();
    lfs_printf ("lookup id=%d,off=%d tid=%u\n", (int) id, (int) offset,
		(unsigned int) tid);
#endif
    obj = __arc_hash_lookup (cache, id, offset, &hash_lock);
    if (obj && obj->read_state == READ_FINISHED)
      {
//        lfs_printf_debug("find in arc\n");
	  arc_stat_hit_update (cache);
	  if (obj->state == &cache->mru || obj->state == &cache->mfu)
	    {
		/* Object is already in the cache, move it to the head of the
		 * MFU list. */
		if (obj->state == &cache->mru ||
		    (obj->state == &cache->mfu
		     && (&obj->head != cache->mfu.head.next)))
		  {
//                    print_obj (obj, "move to mfu in lookup");
		      __arc_move (cache, obj, &cache->mfu, OBJ_UNLOCK);
		  }
		mutex_exit (hash_lock, __func__);
//              print_obj (obj, "end in arc_lookup");
//              lfs_printf_debug ("cache hit\n");
		return obj;
	    }
	  else if (obj->state == &cache->mrug)
	    {
		mutex_enter (&cache->arc_lock, __func__, __LINE__);
		cache->p =
		    MIN (cache->c,
			 cache->p + MAX (cache->mfug.size / cache->mrug.size,
					 1));
		mutex_exit (&cache->arc_lock, __func__);
		__arc_replace (cache, 0);
/* arc has to fetch data from disk.
 */
		pobj = pobject_create (item);
		pending_hash_insert (pobj);
		__arc_move (cache, obj, &cache->mfu, OBJ_UNLOCK);
		mutex_exit (hash_lock, __func__);
		return obj;
	    }
	  else if (obj->state == &cache->mfug)
	    {
		mutex_enter (&cache->arc_lock, __func__, __LINE__);
		cache->p =
		    MAX (0,
			 cache->p - MAX (cache->mrug.size / cache->mfug.size,
					 1));
		mutex_exit (&cache->arc_lock, __func__);
		__arc_replace (cache, IN_MFUG);
/* arc has to fetch data from disk.
 */
		pobj = pobject_create (item);
		pending_hash_insert (pobj);

		__arc_move (cache, obj, &cache->mfu, OBJ_UNLOCK);
		mutex_exit (hash_lock, __func__);
		return obj;
	    }
	  else
	    {
		//print_state(cache);
		lfs_printf ("err:in arc_lookup,obj=%p,obj->state=%p,id=%"
			    PRIu64 ",off=%" PRIu64 "\n", obj, obj->state, id,
			    offset);
		lfs_printf ("hits %" PRIu64 ",total=%" PRIu64 "",
			    cache->arc_stats.hits, cache->arc_stats.total);
		exit (1);
	    }
      }
    else
      {
	  /*data miss */
	  if (obj == NULL)
	    {
		arc_stat_update (cache);
		obj = cache->ops->create (id, offset);
		__arc_hash_insert (cache, key, obj, hash_lock);
		__arc_adjust (cache);	//obj->size

//              print_obj (obj, "first insert");
/*  wait I/O return
 */
		pobj = pobject_create (item);
		pending_hash_insert (pobj);
/*  async fetch data to disk, non-block I/O here.
 */
		cache->ops->fetch_from_disk (id, offset, obj);
		return NULL;
	    }
	  else if (obj != NULL
		   && (obj->read_state == READ_STATE
		       || obj->read_state == READ_HALF_FINISHED))
	    {
		mutex_exit (hash_lock, __func__);
//              print_obj (obj, __LINE__);

		pobj = pobject_create (item);
		pending_hash_insert (pobj);
		return NULL;
		/* New objects are always moved to the MRU list. */
	    }
	  else
	    {
		lfs_printf ("in arc_lookup obj=%p,obj->read_state=%d", obj,
			    obj->read_state);
		assert (0);
	    }
      }
    assert (0);
    return obj;
}
