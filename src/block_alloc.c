#include "block_alloc.h"
#include <stdio.h>
#include <stdlib.h>

static int space_map_seg_compare(const void *x1, const void *x2)
{
	 space_seg_t *s1 = (space_seg_t *)x1;
	const space_seg_t *s2 = x2;

	if (s1->ss_start < s2->ss_start) {
		if (s1->ss_end > s2->ss_start)
			return (0);
		return (-1);
	}
	if (s1->ss_start > s2->ss_start) {
		if (s1->ss_start < s2->ss_end)
			return (0);
		return (1);
	}
	return (0);
}

static uint64_t
metaslab_block_picker(avl_tree_t *t, uint64_t *cursor, uint64_t size)
{
	space_seg_t *ss, ssearch;
	avl_index_t where;

	ssearch.ss_start = *cursor;
	ssearch.ss_end = *cursor + size;

	ss = avl_find(t, &ssearch, &where);
	if (ss == NULL)
		ss = avl_nearest(t, where, AVL_AFTER);

	while (ss != NULL) {
		uint64_t offset = ss->ss_start;

		if (offset + size <= ss->ss_end) {
			*cursor = offset + size;
			return (offset);
		}
		ss = AVL_NEXT(t, ss);
	}

	/*
	 * If we know we've searched the whole map (*cursor == 0), give up.
	 * Otherwise, reset the cursor to the beginning and try again.
	 */
	if (*cursor == 0)
		return (-1ULL);

	*cursor = 0;
	return (metaslab_block_picker(t, cursor, size));
}
static uint64_t
metaslab_ff_alloc(space_map_t *sm, uint64_t size)
{
	avl_tree_t *t = &sm->sm_root;
	uint64_t *cursor = 0;

	return (metaslab_block_picker(t, cursor, size));
}
int lfs_block_allocate(int size,space_map_t *sm){

	avl_create(&sm->sm_root, space_map_seg_compare,sizeof (space_seg_t), offsetof(struct space_seg, ss_node));
	return 1;
}

void space_map_add(space_map_t *sm, uint64_t start, uint64_t size)
{
	avl_index_t where;
	space_seg_t ssearch, *ss_before, *ss_after, *ss;
	uint64_t end = start + size;
	int merge_before, merge_after;
/*
 * The function should be add mutex lock here.
 */
	ASSERT(start>=sm->sm_start);
	ASSERT(end<=sm->sm_start + sm->sm_size);
	ASSERT(sm->sm_space + size <= sm->sm_size);

	ssearch.ss_start = start;
	ssearch.ss_end = end;
	ss = avl_find(&sm->sm_root, &ssearch, &where);

	if (ss != NULL && ss->ss_start <= start && ss->ss_end >= end) {
		printf("space_map_add error");
		return;
	}

	/* Make sure we don't overlap with either of our neighbors */
	VERIFY(ss == NULL);

	ss_before = avl_nearest(&sm->sm_root, where, AVL_BEFORE);
	ss_after = avl_nearest(&sm->sm_root, where, AVL_AFTER);

	merge_before = (ss_before != NULL && ss_before->ss_end == start);
	merge_after = (ss_after != NULL && ss_after->ss_start == end);

	if (merge_before && merge_after) {
		avl_remove(&sm->sm_root, ss_before);
		ss_after->ss_start = ss_before->ss_start;
		free(ss_before);
		ss = ss_after;
	} else if (merge_before) {
		ss_before->ss_end = end;
		ss = ss_before;
	} else if (merge_after) {
		ss_after->ss_start = start;
		ss = ss_after;
	} else {
		ss = malloc(sizeof (*ss));
		ss->ss_start = start;
		ss->ss_end = end;
		avl_insert(&sm->sm_root, ss, where);
	}


	sm->sm_space += size;
}

void space_map_free(space_map_t *sm, uint64_t start, uint64_t size)
{
	space_map_add(sm, start, size);
}
