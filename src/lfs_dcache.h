#ifndef __LFS_DCACHE_H__
#define __LFS_DCACHE_H__
#include <inttypes.h>
#include <memory.h>
#include <stddef.h>
#include "arc.h"
#include "lfs_ops.h"
/**********************************************************************
 * Simple double-linked list, inspired by the implementation used in the
 * linux kernel.
 */
typedef struct pend_hash dcache_hash_t;

#define __arc_list_entry(ptr, type, field) \
    ((type*) (((char*)ptr) - offsetof(type,field)))

#define __arc_list_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define __arc_list_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)


/* insert into lfs_n: dcache_hash_t dcache;
 */
#define DCACHE lfs_n.dcache
/* insert the pending request in io pending queue.
  */
struct dcache_object
{
    inode_t pinode;
    char *fname;
    inode_t inode;
    struct __arc_list head, hash;
};

typedef struct dcache_object dcache_object_t;
inline int dcache_hash_insert (uint32_t key, dcache_object_t * obj);
void dcache_hash_init ();
uint32_t dcache_hash (const char *str, int len);

int dcache_insert (inode_t pinode, const char *fname, inode_t inode);
int dcache_remove (inode_t pinode, char *fname);
inode_t dcache_lookup (inode_t pinode, const char *fname);
#endif /* __PEND_H__ */
