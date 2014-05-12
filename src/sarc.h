#ifndef __SARC_H__
#define __SARC_H__
#include <inttypes.h>
#include <memory.h>
#include <stddef.h>
#include <pthread.h>
#include "arc.h"
/**********************************************************************
 * Simple double-linked list, inspired by the implementation used in the
 * linux kernel.
 */
/*
struct __arc_list {
    struct __arc_list *prev, *next;
};
*/
struct read_range
{
    int fileid;
    uint64_t offset;
    uint64_t size;
};
typedef struct read_range range_t;
#define __arc_list_entry(ptr, type, field) \
    ((type*) (((char*)ptr) - offsetof(type,field)))

#define __arc_list_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define __arc_list_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)
/*
static inline void
__arc_list_init( struct __arc_list * head )
{
    head->next = head->prev = head;
}


static inline void
__arc_list_insert(struct __arc_list *list, struct __arc_list *prev, struct __arc_list *next)
{
    next->prev = list;
    list->next = next;
    list->prev = prev;
    prev->next = list;
}
static inline void
__arc_list_splice(struct __arc_list *prev, struct __arc_list *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void
__arc_list_remove(struct __arc_list *head)
{
    __arc_list_splice(head->prev, head->next);
    head->next = head->prev = NULL;
}

static inline void
__arc_list_prepend(struct __arc_list *head, struct __arc_list *list)
{
    __arc_list_insert(head, list, list->next);
}
*/

/**********************************************************************
 * All objects are inserted into a hashtable to make the lookup fast.
 */
#define ARC_MUTEXES 256
/*
struct __arc_hash {
    uint64_t size;
	uint64_t ht_mask;
    struct __arc_list *bucket;
	lmutex_t hash_mutexes[ARC_MUTEXES+1];
	uint64_t hm_mask;
};

*/
/**********************************************************************
 * The arc state represents one of the m{r,f}u{g,} lists
 */
struct __sarc_state
{
    unsigned long size;
    lmutex_t state_lock;
    struct __arc_list head;
    unsigned long count;
};

/* This structure represents an object that is stored in the cache. Consider
 * this structure private, don't access the fields directly. When creating
 * a new object, use the __arc_object_init() function to initialize it. */
struct __sarc_object
{
    struct __sarc_state *state;
    struct __arc_list head, hash;
    unsigned long size;
    unsigned long number;
    int pcount;
    lmutex_t obj_lock;
    int read_state;
    pthread_cond_t cv;
};

struct __sarc_ops
{
    /* Return a hash value for the given key. */
    unsigned long (*hash) (uint64_t key);

    /* Compare the object with the key. */
    int (*cmp) (struct __sarc_object * obj, uint64_t id, uint64_t offset);

    /* Create a new object. The size of the new object must be know at
     * this time. Use the __arc_object_init() function to initialize
     * the __arc_object structure. */
    struct __sarc_object *(*create) (uint64_t id, uint64_t offset);

    /* Fetch the data associated with the object. */
    int (*fetch) (struct __sarc_object * obj);

    /* This function is called when the cache is full and we need to evict
     * objects from the cache. Free all data associated with the object. */
    void (*evict) (struct __sarc_object * obj);
    /* The function after create a object, it fetch data from disk.
     */
    int (*fetch_from_disk) (uint64_t id, uint64_t offset,
			    struct __sarc_object * obj);
    /* This function is called when the object is completely removed from
     * the cache directory. Free the object data and the object itself. */
    void (*destroy) (struct __sarc_object * obj);
    int (*prefetch_from_disk) (uint64_t id, uint64_t offset,
			       struct __sarc_object * obj);
};
/*
typedef struct arc_stat{
	lmutex_t stat_lock;
	uint64_t hits;
	uint64_t total;
}arc_stat_t;
*/
/* The actual cache. */
typedef struct __sarc
{
    struct __sarc_ops *ops;
    struct __arc_hash hash;
    double adapt;
    double ratio;
    arc_stat_t arc_stats;
/*  for adaptive 
 */
    int seqThreshold;
    unsigned int seqMiss;
    int tail_stat;
    int desiredSeqListSize;
    uint64_t c, p;
    lmutex_t arc_lock;
    lmutex_t arc_balock;
    struct __sarc_state random, seq;
} sarc_t;
/* Functions to create and destroy the cache. */
struct __sarc *__sarc_create (struct __sarc_ops *ops, unsigned long c);
void __sarc_destroy (struct __sarc *cache);

/* Initialize a new object. To be used from the alloc() op function. */
void __sarc_object_init (struct __sarc_object *obj, uint64_t offset, int id,
			 unsigned long size);

/* Lookup an object in the cache. The cache automatically allocates and
 * fetches the object if it does not exists yet. */
struct __sarc_object *__sarc_lookup (struct __sarc *cache, uint64_t id,
				     uint64_t offset);

#endif /* __SARC_H__ */
