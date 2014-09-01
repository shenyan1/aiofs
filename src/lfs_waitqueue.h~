#ifndef __PQUEUE_H__
#define __PQUEUE_H__
#include <inttypes.h>
#include <memory.h>
#include <stddef.h>
#include "arc.h"
#include "lfs_ops.h"
/**********************************************************************
 * Simple double-linked list, inspired by the implementation used in the
 * linux kernel.
 */
typedef struct pend_hash pend_hash_t;
struct pend_hash
{
    uint64_t size;
    uint64_t ht_mask;
    struct __arc_list *bucket;
    lmutex_t phash_mutexes[ARC_MUTEXES + 1];
    uint64_t hm_mask;
};

#define __arc_list_entry(ptr, type, field) \
    ((type*) (((char*)ptr) - offsetof(type,field)))

#define __arc_list_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define __arc_list_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)



#define PQUEUE lfs_n.pending_queue
/* insert the pending request in io pending queue.
  */
struct pend_object
{
    uint64_t id;
    int clifd;
    uint64_t off;
    struct __arc_list head, hash;
};

typedef struct pend_object pend_object_t;
inline void pending_hash_insert (pend_object_t * obj);
void pend_hash_init ();
inline int pending_hash_remove (struct object *obj);

pend_object_t *pobject_create (CQ_ITEM * item);
#endif /* __PEND_H__ */
