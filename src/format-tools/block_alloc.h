#ifndef _BLOCK_ALLOC_H
#define _BLOCK_ALLOC_H
#include"avl.h"
#include"lfs.h"
#include<stddef.h>

typedef struct space_map {
	avl_tree_t	sm_root;	/* AVL tree of map segments */
	uint64_t	sm_space;	/* sum of all segments in the map */
	uint64_t	sm_start;	/* start of map */
	uint64_t	sm_size;	/* size of map */
	uint8_t		sm_shift;	/* unit shift */
	uint8_t		sm_pad[3];	/* unused */
	uint8_t		sm_loaded;	/* map loaded? */
	uint8_t		sm_loading;	/* map loading? */
	void		*sm_ppd;	/* picker-private data */
	kmutex_t	*sm_lock;	/* pointer to lock that protects map */
} space_map_t;

#define MUTEX_HELD(m)	((m)->m_owner == curthread)

typedef struct space_seg {
	avl_node_t	ss_node;	/* AVL node */
	avl_node_t	ss_pp_node;	/* AVL picker-private node */
	uint64_t	ss_start;	/* starting offset of this segment */
	uint64_t	ss_end;		/* ending offset (non-inclusive) */
}space_seg_t;
#endif
