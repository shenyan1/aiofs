#include "arc.h"
#include "lfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "lfs_thread.h"
#include "lfs_ops.h"
#include "lfs_cache.h"

uint64_t lfs_arc_hash(uint64_t id,uint64_t offset){
    
    uint64_t crc = 0;
        
    crc = (id << 5 ) ^ offset;
    return crc;
}
/*
 * To make the arc hash table huge. image the big table can contains 
 * all the 1M block in memory.
 */
void print_state(struct __arc *cache){
	printf("mru:%p,mfu:%p,mrug=%p,mfug=%p\n",&cache->mru,&cache->mfu,&cache->mrug,&cache->mfug);
}
void printsize(struct __arc *cache){
#ifdef LFS_DEBUG
	printf("\ncache->c=%"PRIu64",cache->p=%"PRIu64"\n",cache->c,cache->p);
	printf("mru  size=%"PRIu64"\n",cache->mru.size);
	printf("mrug state size=%"PRIu64"\n",cache->mrug.size);
	printf("mfu state size=%"PRIu64"\n",cache->mfu.size);
	printf("mfug state size=%"PRIu64"\n",cache->mfug.size);
#endif
}
void print_obj(struct __arc_object *obj,char *str){
	struct object *objc;
	objc = getobj(obj);
#ifdef LFS_DEBUG
	printf("%s: obj=%p offset=%"PRIu64",id=%"PRIu64"\n",str,obj,objc->offset,objc->id);
#endif
}
uint64_t arc_hash_init()
{
    uint64_t hsize=1ULL<<12;
    uint64_t physmem = 0;
    physmem = getphymemsize();
    while(hsize * (1<<20) < physmem ){
        hsize <<= 1;
    }
    return hsize;
}       
/* A simple hashtable with fixed bucket count. */
static void __arc_hash_init(struct __arc *cache)
{
	uint64_t i;
	cache->hash.size = arc_hash_init();
	printf("hash size = %"PRIu64"\n",cache->hash.size);

	cache->hash.ht_mask = cache->hash.size - 1;
	cache->hash.bucket = malloc(cache->hash.size * sizeof(struct __arc_list));

	for (i = 0; i < cache->hash.size; ++i) {
		__arc_list_init(&cache->hash.bucket[i]);
	}
}

static inline void __arc_hash_insert(struct __arc *cache, uint64_t key, struct __arc_object *obj,lmutex_t *hash_lock)
{
	unsigned long hash = cache->ops->hash(key) & cache->hash.ht_mask;
	struct __arc_list *iter;
	struct object *objc=getobj(obj);
	uint64_t id,offset;
	id = objc->id;
	offset = objc->offset;
	__arc_list_each(iter, &cache->hash.bucket[hash]) {
		struct __arc_object *obj = __arc_list_entry(iter, struct __arc_object, hash);
		if (cache->ops->cmp(obj, id,offset) == 1){
			printf("existed in hash when insert,line:%d\n",__LINE__);
			mutex_exit(hash_lock,__func__);
			exit(1);
			return ;
		}
	}

	__arc_list_prepend(&obj->hash, &cache->hash.bucket[hash]);
	mutex_exit(hash_lock,__func__);
}

static struct __arc_object *__arc_hash_lookup(struct __arc *cache, uint64_t id,uint64_t offset,lmutex_t **mem_lock)
{
	struct __arc_list *iter;
	uint64_t key;
	key = lfs_arc_hash(id,offset);
	unsigned long hash = cache->ops->hash(key) & cache->hash.ht_mask;
	lmutex_t *hash_lock = &cache->hash.hash_mutexes[hash];
	*mem_lock = hash_lock;
	mutex_enter(hash_lock,__func__);
	__arc_list_each(iter, &cache->hash.bucket[hash]) {
		struct __arc_object *obj = __arc_list_entry(iter, struct __arc_object, hash);
		if (cache->ops->cmp(obj, id,offset) == 1){
		//	mutex_exit(hash_lock);
			return obj;
		}
	}

	return NULL;
}

static void __arc_hash_fini(struct __arc *cache)
{
	free(cache->hash.bucket);
}

/* Initialize a new object with this function. */
void __arc_object_init(struct __arc_object *obj, unsigned long size)
{

	mutex_init(&obj->obj_lock);
	obj->state = NULL;
	obj->size = size;
	obj->read_state = READ_STATE;
	assert(pthread_cond_init(&obj->cv, NULL)== 0);
	__arc_list_init(&obj->head);
	__arc_list_init(&obj->hash);
}

static inline unsigned long gethash_lock(struct __arc *cache,struct __arc_object *_obj){
	struct object *obj = getobj(_obj);
	uint64_t id,offset,key;
	unsigned long hash;
	id = obj->id;
	offset = obj->offset;
	key = lfs_arc_hash(id,offset);
	hash = cache->ops->hash(key) & cache->hash.ht_mask;
	return hash;
}
/* Forward-declaration needed in __arc_move(). */
//static void __arc_balance(struct __arc *cache, unsigned long size);

static struct __arc_object *__arc_state_lru(struct __arc_state *state)
{
    struct __arc_list *head = state->head.prev;
    return __arc_list_entry(head, struct __arc_object, head);
}

static struct __arc_object *__arc_move_state(struct __arc *cache,struct __arc_state *state1,struct __arc_state *state2)
{
	struct __arc_object *obj;
	mutex_enter(&state1->state_lock,__func__);
	obj = __arc_state_lru(state1);	
    state1->size -= obj->size;
	state1->count--;
	printsize(cache);
#ifdef LFS_DEBUG
	printf("state have %lu objs",state1->count);
	print_obj(obj,"arc_move_state:");
#endif
	if(obj->state == NULL)
		exit(1);
#ifdef LFS_DEBUG
		//print_state(cache);
	//	printf("obj=%p,obj->state = %p\n",obj,obj->state);
		if(obj->state == &cache->mfu){
			printf("remove from mfu,obj=%p,nums:%u\n",obj,obj->state->count);
			if(state == &cache->mfu)
				printf("mfu to mfu,obj:prev=%llu,next=%llu,obj->head=%llu\n",(unsigned long long)obj->head.prev,(unsigned long long)obj->head.next,(unsigned long long)&obj->head);
			if(state == &cache->mfug)
				printf("move from mfu to mfug\n");
       	}

#endif
		 __arc_list_remove(&obj->head);
		mutex_exit(&obj->state->state_lock,__func__);

    if (state2 == NULL) {
		lmutex_t *hash_lock;
		unsigned long hash= gethash_lock(cache,obj);
		assert(hash < ARC_MUTEXES);
		hash_lock = &cache->hash.hash_mutexes[hash];

        /* The object is being removed from the cache, destroy it. */
		mutex_enter(hash_lock,__func__);
        __arc_list_remove(&obj->hash);
		printf("move to null list");
        cache->ops->destroy(obj);
		mutex_exit(hash_lock,__func__);
		return NULL;
    } else {
        if (state2 == &cache->mrug || state2 == &cache->mfug) {
            /* The object is being moved to one of the ghost lists, evict
             * the object from the cache. */
#ifdef LFS_DEBUG
			if(state2 == &cache->mrug)
				printf("evict:moving into the mrug ");
			else printf("evict:moving into the mfug ");
			printf("arc_move_state:in cache_alloc evict");

#endif
            cache->ops->evict(obj);
        } else if (state1 != &cache->mru && state1 != &cache->mfu) {
            /* The object is being moved from one of the ghost lists into
             * the MRU or MFU list, fetch the object into the cache. */
            //__arc_balance(cache, obj->size);
			assert(state1 == &cache->mrug || state1 == &cache->mfug);
			printf("arc_move state:in cache_alloc\n");
            if (cache->ops->fetch(obj)) {
                /* If the fetch fails, put the object back to the list
                 * it was in before. */
				assert(0);
				mutex_enter(&state1->state_lock,__func__);
                obj->state->size += obj->size;
				obj->state->count ++;
                __arc_list_prepend(&obj->head, &obj->state->head);
                
				mutex_exit(&state1->state_lock,__func__);
				printf("fetch failed in arc_move\n");
                return NULL;
            }
        }
		mutex_enter(&state2->state_lock,__func__);
        __arc_list_prepend(&obj->head, &state2->head);
        obj->state = state2;
        obj->state->size += obj->size;
		assert(state2 != &cache->mru);
		assert(state2 != &cache->mfu);
		obj->state->count++;
		mutex_exit(&state2->state_lock,__func__);
    }
    
    return obj;
}


/* Move the object to the given state. If the state transition requires,
* fetch, evict or destroy the object. */
static struct __arc_object *__arc_move(struct __arc *cache, struct __arc_object *obj, struct __arc_state *state,int flag)
{
    if (obj->state) {
		if(flag == OBJ_UNLOCK)
			mutex_enter(&obj->state->state_lock,__func__);
        obj->state->size -= obj->size;
		//printf("obj's size=%"PRIu64",state's size=%"PRIu64"\n",obj->size,obj->state->size);
		obj->state->count--;
#ifdef LFS_DEBUG
		printf("obj=%p,obj->state = %p\n",obj,obj->state);
		//print_state(cache);
		if(obj->state == &cache->mfu){
			printf("remove from mfu,obj=%p,nums:%u\n",obj,obj->state->count);
			if(state == &cache->mfu)
				printf("mfu to mfu,obj:prev=%llu,next=%llu,obj->head=%llu\n",(unsigned long long)obj->head.prev,(unsigned long long)obj->head.next,(unsigned long long)&obj->head);
			if(state == &cache->mfug)
				printf("move from mfu to mfug\n");
       	}

#endif
		 __arc_list_remove(&obj->head);
		mutex_exit(&obj->state->state_lock,__func__);
    }

    if (state == NULL) {
		lmutex_t *hash_lock;
		unsigned long hash= gethash_lock(cache,obj);
		assert(hash < ARC_MUTEXES);
		hash_lock = &cache->hash.hash_mutexes[hash];

        /* The object is being removed from the cache, destroy it. */
		mutex_enter(hash_lock,__func__);
        __arc_list_remove(&obj->hash);
        cache->ops->destroy(obj);
		mutex_exit(hash_lock,__func__);
		return NULL;
    } else {
        if (state == &cache->mrug || state == &cache->mfug) {
            /* The object is being moved to one of the ghost lists, evict
             * the object from the cache. */

#ifdef LFS_DEBUG
			if(state == &cache->mrug)
				printf("moving into the mrug ");
			else printf("moving into the mfug ");
#endif
			printf("arc_move:in cache_alloc evict");
            cache->ops->evict(obj);
        } else if (obj->state != &cache->mru && obj->state != &cache->mfu &&flag != OBJ_INIT) {
            /* The object is being moved from one of the ghost lists into
             * the MRU or MFU list, fetch the object into the cache. */
            //__arc_balance(cache, obj->size);

			assert(obj->state !=NULL);
			assert(obj->state == &cache->mrug || obj->state == &cache->mfug);
            if (cache->ops->fetch(obj)) {
                /* If the fetch fails, put the object back to the list
                 * it was in before. */
				assert(0);
				mutex_enter(&state->state_lock,__func__);
                obj->state->size += obj->size;
				obj->state->count ++;
                __arc_list_prepend(&obj->head, &obj->state->head);
                
				mutex_exit(&state->state_lock,__func__);
#ifdef LFS_DEBUG
				printf("fetch failed in arc_move\n");
#endif
                return NULL;
            }
        }
		mutex_enter(&state->state_lock,__func__);
        __arc_list_prepend(&obj->head, &state->head);
        obj->state = state;
        obj->state->size += obj->size;
		
		obj->state->count++;
#ifdef LFS_DEBUG
		if(state == &cache->mfu)
			printf("going to move to mfu obj=%p,nums=%lu\n",
				obj,state->count);	
#endif
		if(state == &cache->mru)
			;//printf("become mru in arc_move obj=%p\n ",obj);
		mutex_exit(&state->state_lock,__func__);
    }
    
    return obj;
}

/* Return the LRU element from the given state. */
static void __arc_replace(struct __arc *cache,int ismfug){
	if((cache->mru.size!=0 && cache->mru.size >cache->p) || (ismfug!=0 && cache->mru.size ==cache->p) ){
		__arc_move_state(cache,&cache->mru,&cache->mrug);
	}
	else{
		__arc_move_state(cache,&cache->mfu,&cache->mfug);
	}
}
static void __arc_adjust(struct __arc *cache){
	printsize(cache);
	/* case A:
	 */
	if(cache->mru.size + cache->mrug.size == cache->c){
		if(cache->mru.size < cache->c){
			struct __arc_object *obj= __arc_state_lru(&cache->mrug);
			assert(obj->state == &cache->mrug);
			__arc_move_state(cache,&cache->mrug,NULL);
			__arc_replace(cache,0);
		}else {
			assert(cache->mrug.size ==0);
			struct __arc_object *obj= __arc_state_lru(&cache->mru);
			assert(obj->state == &cache->mru);
			__arc_move_state(cache,&cache->mru,NULL);
		}
	}
	/* case B:*/
	if(cache->mru.size + cache->mrug.size < cache->c ){
		if(cache->mru.size + cache->mrug.size + cache->mfu.size + cache->mfug.size >=cache->c ){
			if(cache->mru.size + cache->mfu.size + cache->mrug.size + cache->mfug.size == 2*cache->c)
				__arc_move_state(cache,&cache->mfug,NULL);
			__arc_replace(cache,0);

		}
	}
}
/* Balance the lists so that we can fit an object with the given size into
 * the cache. */
#if 0
static void __arc_balance(struct __arc *cache, unsigned long size)
{
	//printf("going to balance in arc\n");
    /* First move objects from MRU/MFU to their respective ghost lists. */
	printsize(cache);
    while (cache->mru.size + cache->mfu.size + size > cache->c) {        
        if (cache->mru.size > cache->p) {
		//	mutex_enter(&cache->mru.state_lock);
            struct __arc_object *obj = __arc_state_lru(&cache->mru);

			printf("put the obj %p state =%p into mrug,%d\n",obj,obj->state,(int)pthread_self());
			assert(obj->state == &cache->mru);
            __arc_move(cache, obj, &cache->mrug,OBJ_UNLOCK);
			printf("move finished obj->state =%p\n",obj->state);
        } else if (cache->mfu.size > 0) {
			printf("in while go to balance tid:%d\n",(int)pthread_self());
			printsize(cache);

            struct __arc_object *obj = __arc_state_lru(&cache->mfu);
			assert(&cache->mfu == obj->state);
            __arc_move(cache, obj, &cache->mfug,OBJ_UNLOCK);

        } else {
            break;
        }
    }
    /* Then start removing objects from the ghost lists. */
    while (cache->mrug.size + cache->mfug.size > cache->c) {        
        if (cache->mfug.size > cache->p) {
            struct __arc_object *obj = __arc_state_lru(&cache->mfug);
            __arc_move(cache, obj, NULL,OBJ_UNLOCK);
        } else if (cache->mrug.size > 0) {
		//	mutex_enter(&cache->mrug.state_lock);
            struct __arc_object *obj = __arc_state_lru(&cache->mrug);
            __arc_move(cache, obj, NULL,OBJ_UNLOCK);
        } else {
            break;
        }
    }
	//printf("end arc balance\n");
}
#endif

/* Create a new cache. */
struct __arc *__arc_create(struct __arc_ops *ops, uint64_t c)
{
	int i;
    struct __arc *cache = malloc(sizeof(struct __arc));
    memset(cache, 0, sizeof(struct __arc));

    cache->ops = ops;
    
    __arc_hash_init(cache);
	c -= c % LFS_BLKSIZE ;

    cache->c = c;
    cache->p = c >> 1;

	cache->arc_stats.hits = 0;
	cache->arc_stats.total = 0;
	mutex_init(&cache->arc_stats.stat_lock);
	mutex_init(&cache->arc_lock);
	mutex_init(&cache->arc_balock);
    __arc_list_init(&cache->mrug.head);
    __arc_list_init(&cache->mru.head);
    __arc_list_init(&cache->mfu.head);
    __arc_list_init(&cache->mfug.head);
/*  the last hash_mutexes is reserved for list_remove
 */
	for(i = 0; i <= ARC_MUTEXES; i++) {
		mutex_init(&cache->hash.hash_mutexes[i]);
	}
	cache->hash.ht_mask = ARC_MUTEXES - 1;

	mutex_init(&cache->mrug.state_lock);
	mutex_init(&cache->mru.state_lock);
	mutex_init(&cache->mfu.state_lock);
	mutex_init(&cache->mfug.state_lock);
	print_state(cache);
    return cache;
}

static inline void arc_list_destroy(struct __arc_state *state,struct __arc *cache){

	struct __arc_object *obj;
	while(state->head.prev!=state->head.next && state->head.prev !=NULL){
    	obj = __arc_state_lru(state);
		__arc_list_remove(&obj->head);
		cache->ops->destroy(obj);
		
	}
}
/* Destroy the given cache. Free all objects which remain in the cache. */
void __arc_destroy(struct __arc *cache)
{
 //   struct __arc_list *iter;
    int i;
	arc_list_destroy(&cache->mrug,cache);	
	printf("finish the mrug\n");
	arc_list_destroy(&cache->mru,cache);	
	printf("finish the mru\n");
	arc_list_destroy(&cache->mfu,cache);	
	printf("finish the mfu\n");
	arc_list_destroy(&cache->mfug,cache);	
	printf("finish the mfug\n");

	for(i = 0; i <= ARC_MUTEXES; i++) {
		mutex_destroy(&cache->hash.hash_mutexes[i]);
	}

    __arc_hash_fini(cache);
	mutex_destroy(&cache->arc_balock);
	mutex_destroy(&cache->arc_lock);
	mutex_destroy(&cache->arc_stats.stat_lock);
	mutex_destroy(&cache->mrug.state_lock);
    mutex_destroy(&cache->mru.state_lock);
    mutex_destroy(&cache->mfu.state_lock);
    mutex_destroy(&cache->mfug.state_lock);
    free(cache);
}


static inline void arc_stat_hit_update(struct __arc *cache){
	mutex_enter(&cache->arc_stats.stat_lock,__func__);
	cache->arc_stats.hits++;
	cache->arc_stats.total++;
	mutex_exit(&cache->arc_stats.stat_lock,__func__);
}

static inline void arc_stat_update(struct __arc *cache){

	mutex_enter(&cache->arc_stats.stat_lock,__func__);
	cache->arc_stats.total++;
	mutex_exit(&cache->arc_stats.stat_lock,__func__);
}
inline void arc_read_done(struct __arc_object *obj){
	mutex_enter(&obj->obj_lock,__func__);
	if(obj->read_state == READ_STATE){
		obj->read_state = READ_FINISHED;
		cv_broadcast(&obj->cv);
	}
	mutex_exit(&obj->obj_lock,__func__);
}
/* Lookup an object with the given key. */
struct __arc_object *__arc_lookup(struct __arc *cache, uint64_t id, uint64_t offset)
{
#define IN_MFUG 1
    struct __arc_object *obj ;
	lmutex_t *hash_lock;
	uint64_t key;
	key = lfs_arc_hash(id,offset);
#ifdef LFS_DEBUG

	pthread_t tid=pthread_self();
	printf("lookup id=%d,off=%d tid=%u\n",(int)id,(int)offset,(unsigned int)tid);
#endif
	obj = __arc_hash_lookup(cache, id,offset,&hash_lock);
    if (obj && obj->read_state == READ_FINISHED ) {
		arc_stat_hit_update(cache);
        if (obj->state == &cache->mru || obj->state == &cache->mfu) {
            /* Object is already in the cache, move it to the head of the
             * MFU list. */
			if(obj->state == &cache->mru ||
					 (obj->state == &cache->mfu && (&obj->head != cache->mfu.head.next))){
				print_obj(obj,"move to mfu in lookup");
				__arc_move(cache, obj, &cache->mfu,OBJ_UNLOCK);
				}
			mutex_exit(hash_lock,__func__);
			print_obj(obj,"end in arc_lookup\n");
            return  obj;
       } else if (obj->state == &cache->mrug) {
			mutex_enter(&cache->arc_lock,__func__);
            cache->p = MIN(cache->c, cache->p + MAX(cache->mfug.size / cache->mrug.size, 1));
			mutex_exit(&cache->arc_lock,__func__);
			__arc_replace(cache,0);
			__arc_move(cache, obj, &cache->mfu,OBJ_UNLOCK);
			mutex_exit(hash_lock,__func__);
			return obj;
        } else if (obj->state == &cache->mfug) {
			mutex_enter(&cache->arc_lock,__func__);
            cache->p = MAX(0, cache->p - MAX(cache->mrug.size / cache->mfug.size, 1));
			mutex_exit(&cache->arc_lock,__func__);
			__arc_replace(cache,IN_MFUG);
			__arc_move(cache, obj, &cache->mfu,OBJ_UNLOCK);
			mutex_exit(hash_lock,__func__);
			return  obj;
       } else {
			//print_state(cache);
			//printf("in arc_lookup,obj=%p,obj->state=%p,id=%"PRIu64",off=%"PRIu64"\n",obj,obj->state,id,offset);
			printf("hits %"PRIu64",total=%"PRIu64"",cache->arc_stats.hits,cache->arc_stats.total);
            exit(1);
        }
    } else {
	/*data miss */
		if(obj==NULL){
			arc_stat_update(cache);
 		 	obj = cache->ops->create(id,offset);
#ifdef LFS_DEBUG
			printf("going to insert hash arc_lookup\n");
#endif
        		__arc_hash_insert(cache, key, obj,hash_lock);
            		__arc_adjust(cache);//obj->size
      
			cache->ops->fetch_from_disk(id,offset,obj);
        		if (!obj)
            			return NULL;
        		obj = __arc_move(cache, obj, &cache->mru,OBJ_INIT);
			assert(obj->state == &cache->mru);
			assert(obj->read_state == READ_STATE);
			arc_read_done(obj);
			return obj;
		}else if(obj!=NULL && obj->read_state == READ_STATE){
				mutex_exit(hash_lock,__func__);

				mutex_enter(&obj->obj_lock,__func__);
#ifdef LFS_DEBUG
				printf("going to wait\n");
#endif
				while(obj->read_state == READ_STATE){
					cv_wait(&obj->cv,&obj->obj_lock);
				}
				mutex_exit(&obj->obj_lock,__func__);
				return obj;
			/* New objects are always moved to the MRU list. */
		}else{
			printf("in arc_lookup obj=%p,obj->read_state=%d",obj,obj->read_state);
			assert(0);
		}
    }
		assert(0);
		//printf("end arc Look up obj=%p,id=%"PRIu64",offset=%"PRIu64"\n",obj,id,offset);
		return obj;
}
