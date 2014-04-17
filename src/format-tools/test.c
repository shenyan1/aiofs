
#include "arc.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

/* This is the object we're managing. It has a name (sha1)
* and some data. This data will be loaded when ARC instruct
* us to do so. */
struct object {
	uint64_t id;
	uint64_t offset;
	struct __arc_object entry;
	void *data;
};
#define NUMS 5
unsigned char objname(struct __arc_object *entry)
{
	struct object *obj = __arc_list_entry(entry, struct object, entry);
	return obj->id;
}


/**
* Here are the operations implemented
*/

static unsigned long __op_hash(uint64_t key)
{
	return key;
}

static int __op_compare(struct __arc_object *e, uint64_t id,uint64_t offset)
{
	struct object *obj = __arc_list_entry(e, struct object, entry);
	return obj->id == id && obj->offset == offset;
}

static struct __arc_object *__op_create(uint64_t id,uint64_t offset)
{
	struct object *obj = malloc(sizeof(struct object));
	obj->offset = offset;
	obj->id = id;
/*
 * At this time we read from our arc buffer. if the buffer isn't in arc,we call this,so read from disk now.
 */
	//read from disk to obj->data
	obj->data = NULL;

	__arc_object_init(&obj->entry, 1<<20);
	return &obj->entry;
}

static int __op_fetch(struct __arc_object *e)
{
	struct object *obj = __arc_list_entry(e, struct object, entry);
	obj->data = malloc(200);
	return 0;
}

static void __op_evict(struct __arc_object *e)
{
	struct object *obj = __arc_list_entry(e, struct object, entry);
	free(obj->data);
}

static void __op_destroy(struct __arc_object *e)
{
	struct object *obj = __arc_list_entry(e, struct object, entry);
	free(obj);
}

static struct __arc_ops ops = {
	.hash		= __op_hash,
	.cmp		= __op_compare,
	.create		= __op_create,
	.fetch		= __op_fetch,
	.evict		= __op_evict,
	.destroy	= __op_destroy
};

static void stats(struct __arc *s)
{
	int i = 0;
	struct __arc_list *pos;

	__arc_list_each_prev(pos, &s->mrug.head) {
		struct __arc_object *e = __arc_list_entry(pos, struct __arc_object, head);
		assert(e->state == &s->mrug);
		struct object *obj = __arc_list_entry(e, struct object, entry);
		printf("[%02x,%02x]", obj->id,obj->offset);
	}
	printf(" + ");
	__arc_list_each_prev(pos, &s->mru.head) {
		struct __arc_object *e = __arc_list_entry(pos, struct __arc_object, head);
		assert(e->state == &s->mru);
		struct object *obj = __arc_list_entry(e, struct object, entry);
		printf("[%02x,%02x]", obj->id,obj->offset);

		if (i++ == s->p)
			printf(" # ");
	}
	printf(" + ");
	__arc_list_each(pos, &s->mfu.head) {
		struct __arc_object *e = __arc_list_entry(pos, struct __arc_object, head);
		assert(e->state == &s->mfu);
		struct object *obj = __arc_list_entry(e, struct object, entry);
		printf("[%02x,%02x]", obj->id,obj->offset);

		if (i++ == s->p)
			printf(" # ");
	}
	if (i == s->p)
		printf(" # ");
	printf(" + ");
	__arc_list_each(pos, &s->mfug.head) {
		struct __arc_object *e = __arc_list_entry(pos, struct __arc_object, head);
		assert(e->state == &s->mfug);
		struct object *obj = __arc_list_entry(e, struct object, entry);
		printf("[%02x,%02x]", obj->id,obj->offset);
	}
	printf("\n");
}

#define MAXOBJ 16

int main(int argc, char *argv[])
{
	srandom(time(NULL));
	int i;
	uint64_t sha1[MAXOBJ][20];
	for ( i = 0; i < MAXOBJ; ++i) {
		memset(sha1[i], 0, 20);
		sha1[i][0] = i;
	}

	struct __arc *s = __arc_create(&ops, 3000);

	for ( i = 0; i < 4 * MAXOBJ; ++i) {
		uint64_t cur[2];
		cur[0] = random()%NUMS;
		cur[1] = random()%NUMS;
		
		printf("get %02x:%02x ", cur[0],cur[1]);
		assert(__arc_lookup(s, cur[0],cur[1]));
		stats(s);
	}


	return 0;
}
