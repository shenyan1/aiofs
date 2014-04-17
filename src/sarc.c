#include "sarc.h"
#include "lfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "lfs_thread.h"
#include "lfs_ops.h"
#include "lfs_cache.h"
/*  Sequencial Prefetching in Adaptive Replacement Cache implemention
    @author: shenyan
 */
#ifdef USE_SARC
extern lfs_info_t lfs_n;
#define LARGE_RATIO 10
struct __sarc ;
struct __sarc_object *__sarc_hash_lookup(uint64_t offset,uint64_t id,lmutex_t **mem_lock);
struct object *sarc_getobj(struct __sarc_object *e);
inline void __sarc_hash_insert(struct __sarc *cache, uint64_t key, struct __sarc_object *obj,lmutex_t *hash_lock);
/*
 * To make the arc hash table huge. image the big table can contains 
 * all the 1M block in memory.
 */
static void printsize(sarc_t *cache){
#ifdef LFS_DEBUG
	printf("\ncache->c=%"PRIu64",cache->p=%"PRIu64"\n",cache->c,cache->p);
	printf("seq state size=%"PRIu64"\n",cache->seq.size);
	printf("random state size=%"PRIu64"\n",cache->random.size);
#endif
}
void sarc_move_state(struct __sarc_object *obj){
	struct __sarc_state *state;
	if(obj->pcount!=lfs_n.sarc_cache->seqThreshold){
		state = &lfs_n.sarc_cache->rand;
	}
	else    state = &lfs_n.sarc_cache->seq;
	mutex_enter(&state->state_lock,__func__);
	obj->state->size += obj->size;
	obj->state->count ++;
        __arc_list_prepend(&obj->head, &obj->state->head);
	mutex_exit(&state->state_lock,__func__);
}
void readandmru(uint64_t id,uint64_t offset){
	int id=range.fileid;
	struct __sarc_object *obj ;
        lmutex_t *hash_lock;
        uint64_t key;
	uint64_t off,end=range.size+range.offset;
	key = lfs_arc_hash(id,off);
        obj = __sarc_hash_lookup(off,id,&hash_lock);
	if(obj==NULL){
		obj = lfs_n.sarc_cache->ops->create(id,off);
                printf("going to insert hash \n");
                __sarc_hash_insert(lfs_n.sarc_cache, key, obj,hash_lock);
		cache->ops->prefetch_from_disk(id,offset,obj);
		assert(obj);
		assert(obj->read_state == READ_STATE);
		sarc_read_done(obj);
		return obj;
	}
	else if(obj!=NULL&&obj->read_state ==READ_STATE){
		mutex_exit(hash_lock,__func__);
		mutex_enter(&obj->obj_lock,__func__);
		printf("going to wait\n");
		while(obj->read_state == READ_STATE){
			cv_wait(&obj->cv,&obj->obj_lock);
		}
		mutex_exit(&obj->obj_lock,__func__);
		return obj;

	}
	

}
static void print_obj(struct __sarc_object *obj,char *str){
	struct object *objc;
	objc = sarc_getobj(obj);
#ifdef LFS_DEBUG
	printf("%s: obj=%p offset=%"PRIu64",id=%"PRIu64"\n",str,obj,objc->offset,objc->id);
#endif
}
       
/* A simple hashtable with fixed bucket count. */
static void __sarc_hash_init(struct __sarc *cache)
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

inline void __sarc_hash_insert(struct __sarc *cache, uint64_t key, struct __sarc_object *obj,lmutex_t *hash_lock)
{
	unsigned long hash = cache->ops->hash(key) & cache->hash.ht_mask;
	struct __arc_list *iter;
	struct object *objc=sarc_getobj(obj);
	uint64_t id,offset;
	id = objc->id;
	offset = objc->offset;
	__arc_list_each(iter, &cache->hash.bucket[hash]) {
		struct __sarc_object *obj = __arc_list_entry(iter, struct __sarc_object, hash);
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
/* to avoid mutex lock twice. we should make sure if mutex_held by myself.
 * no lock implement of sarc_hash_lookup
 */
 struct __sarc_object *__sarc_hash_lookup_nolock(uint64_t offset,uint64_t id)
{
	struct __arc_list *iter;
	uint64_t key;
	key = lfs_arc_hash(id,offset);
	unsigned long hash = lfs_n.sarc_cache->ops->hash(key) & lfs_n.sarc_cache->hash.ht_mask;
	__arc_list_each(iter, &lfs_n.sarc_cache->hash.bucket[hash]) {
		struct __sarc_object *obj = __arc_list_entry(iter, struct __sarc_object, hash);
		if (lfs_n.sarc_cache->ops->cmp(obj, id,offset) == 1){
		//	mutex_exit(hash_lock);
			return obj;
		}
	}

	return NULL;
}
inline lmutex_t *get_lfs_hashlock(uint64_t offset,uint64_t id){

	uint64_t key;
	key = lfs_arc_hash(id,offset);
	unsigned long hash = lfs_n.sarc_cache->ops->hash(key) & lfs_n.sarc_cache->hash.ht_mask;
	return &lfs_n.sarc_cache->hash.hash_mutexes[hash];
}
inline int need_lock_neighbor(uint64_t offset,uint64_t id){
	return get_lfs_hashlock(offset,id)!=get_lfs_hashlock(offset-LFS_BLKSIZE,id);
}

 struct __sarc_object *__sarc_hash_lookup(uint64_t offset,uint64_t id,lmutex_t **mem_lock)
{
	struct __arc_list *iter;
	*mem_lock = get_lfs_hashlock(offset,id);
	mutex_enter(*mem_lock,__func__);
	__arc_list_each(iter, &lfs_n.sarc_cache->hash.bucket[hash]) {
		struct __sarc_object *obj = __arc_list_entry(iter, struct __sarc_object, hash);
		if (lfs_n.sarc_cache->ops->cmp(obj, id,offset) == 1){
		//	mutex_exit(hash_lock);
			return obj;
		}
	}

	return NULL;
}

static void __arc_hash_fini(struct __sarc *cache)
{
	free(cache->hash.bucket);
}

/* Initialize a new object with this function. */
void __sarc_object_init(struct __sarc_object *obj,uint64_t offset, int id,unsigned long size)
{
	int is_locked=false,pc;
	lmutex_t *hash_lock;
	struct __sarc_object *obj_prev;
	mutex_init(&obj->obj_lock);
	obj->state = NULL;
	obj->size = size;
	obj->read_state = READ_STATE;
	is_locked = need_lock_neighbor(offset,id);
	if(is_locked)
		obj_prev = __sarc_hash_lookup_nolock(id,offset-LFS_BLKSIZE);
	else {
		obj_prev= __sarc_hash_lookup(id,offset-LFS_BLKSIZE,&hash_lock);
	}
	if(obj_prev){
		if(obj_prev->pcount==lfs_n.sarc_cache->seqThreshold){
			lfs_n.sarc_cache->seqMiss++;

//			prefetchandmru(id,offset,&lfs_n.arc_cache->seq);
			pc = lfs_n.sarc_cache->seqThreshold;
		}else{
		assert(obj_prev->pcount<lfs_n.sarc_cache->seqThreshold);
		pc = obj_prev->pcount+1;
		}
	}
	else{
/* init the seqcounter
 */
	    pc = 1;

	}
	if(!is_locked){
	/* unlock the neighbor lock due to lookup ops.
	 */
	mutex_exit(hash_lock,__func__);
	}
	obj->pcount = pc;
	assert(pthread_cond_init(&obj->cv, NULL)== 0);
	__arc_list_init(&obj->head);
	__arc_list_init(&obj->hash);
}

static inline unsigned long gethash_lock(struct __sarc *cache,struct __sarc_object *_obj){
	struct object *obj = sarc_getobj(_obj);
	uint64_t id,offset,key;
	unsigned long hash;
	id = obj->id;
	offset = obj->offset;
	key = lfs_arc_hash(id,offset);
	hash = cache->ops->hash(key) & cache->hash.ht_mask;
	return hash;
}
/* Forward-declaration needed in __arc_move(). */

static struct __sarc_object *__arc_state_lru(struct __sarc_state *state)
{
    struct __arc_list *head = state->head.prev;
    return __arc_list_entry(head, struct __sarc_object, head);
}
#if 0
static struct __sarc_object *__arc_move_state(struct __sarc *cache,struct __arc_state *state1,struct __arc_state *state2)
{
	struct __sarc_object *obj;
	mutex_enter(&state1->state_lock,__func__);
	obj = __arc_state_lru(state1);	
    state1->size -= obj->size;
	state1->count--;
	printsize(cache);
	printf("state have %d objs",state1->count);
	print_obj(obj,"arc_move_state:");
	if(obj->state == NULL)
		exit(1);
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

            cache->ops->evict(obj);
        } else if (state1 != &cache->mru && state1 != &cache->mfu) {
            /* The object is being moved from one of the ghost lists into
             * the MRU or MFU list, fetch the object into the cache. */
            if (cache->ops->fetch(obj)) {
                /* If the fetch fails, put the object back to the list
                 * it was in before. */
				assert(0);
				mutex_enter(&state1->state_lock,__func__);
                obj->state->size += obj->size;
				obj->state->count ++;
                __arc_list_prepend(&obj->head, &obj->state->head);
                
				mutex_exit(&state1->state_lock,__func__);
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
#endif
static struct __sarc_object *__sarc_move(struct __sarc *cache, struct __sarc_object *obj, struct __sarc_state *state,int flag)
{
    if (obj->state) {
		if(flag == OBJ_UNLOCK)
			mutex_enter(&obj->state->state_lock,__func__);
        	obj->state->size -= obj->size;
		//printf("obj's size=%"PRIu64",state's size=%"PRIu64"\n",obj->size,obj->state->size);
		obj->state->count--;
		printf("obj=%p,obj->state = %p\n",obj,obj->state);
#ifdef LFS_DEBUG
		//print_state(cache);
		if(obj->state == &cache->random){
			if(state == &cache->random){
			printf("mfu rand , obj=%p,nums:%u\n",obj,obj->state->count);
			}
			else {
			printf("move rand to seq\n");
			}
       		}

#endif
		 __arc_list_remove(&obj->head);
		mutex_exit(&obj->state->state_lock,__func__);
    }
    assert(state != NULL);
    mutex_enter(&state->state_lock,__func__);
    __arc_list_prepend(&obj->head, &state->head);
    obj->state = state;
    obj->state->size += obj->size;
    obj->state->count++;
    mutex_exit(&state->state_lock,__func__);
    
    return obj;
}

#if 0
static void __arc_adjust(struct __sarc *cache){
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
#endif
/* Create a new cache. */
struct __sarc *__sarc_create(struct __sarc_ops *ops, uint64_t c)
{
	int i;
    struct __sarc *cache = malloc(sizeof(struct __sarc));
    memset(cache, 0, sizeof(struct __sarc));

    cache->ops = ops;
    
    __sarc_hash_init(cache);
    c -= c % LFS_BLKSIZE ;
    cache->tail_stat = c*5/100;
    cache->c = c;
    cache->p = c >> 1;
    cache->seqThreshold = 3;
	cache->arc_stats.hits = 0;
	cache->arc_stats.total = 0;
	mutex_init(&cache->arc_stats.stat_lock);
	mutex_init(&cache->arc_lock);
	mutex_init(&cache->arc_balock);
    __arc_list_init(&cache->seq.head);
    __arc_list_init(&cache->random.head);
/*  the last hash_mutexes is reserved for list_remove
 */
	for(i = 0; i <= ARC_MUTEXES; i++) {
		mutex_init(&cache->hash.hash_mutexes[i]);
	}
	cache->hash.ht_mask = ARC_MUTEXES - 1;

	mutex_init(&cache->seq.state_lock);
	mutex_init(&cache->random.state_lock);
	print_state(cache);
    return cache;
}

static inline void list_destroy(struct __sarc_state *state,struct __sarc *cache){

	struct __sarc_object *obj;
	while(state->head.prev!=state->head.next && state->head.prev !=NULL){
    	obj = __arc_state_lru(state);
		__arc_list_remove(&obj->head);
		cache->ops->destroy(obj);
		
	}
}
/* Destroy the given cache. Free all objects which remain in the cache. */
void __sarc_destroy(struct __sarc *cache)
{
 //   struct __arc_list *iter;
    int i;
	list_destroy(&cache->seq,cache);	
	printf("finish the mrug\n");
	list_destroy(&cache->random,cache);	
	printf("finish the mru\n");

	for(i = 0; i <= ARC_MUTEXES; i++) {
		mutex_destroy(&cache->hash.hash_mutexes[i]);
	}

   	 __arc_hash_fini(cache);
	mutex_destroy(&cache->arc_balock);
	mutex_destroy(&cache->arc_lock);
	mutex_destroy(&cache->arc_stats.stat_lock);
	mutex_destroy(&cache->seq.state_lock);
	mutex_destroy(&cache->random.state_lock);
    	free(cache);
}


static inline void arc_stat_hit_update(struct __sarc *cache){
	mutex_enter(&cache->arc_stats.stat_lock,__func__);
	cache->arc_stats.hits++;
	cache->arc_stats.total++;
	mutex_exit(&cache->arc_stats.stat_lock,__func__);
}

static inline void arc_stat_update(struct __sarc *cache){

	mutex_enter(&cache->arc_stats.stat_lock,__func__);
	cache->arc_stats.total++;
	mutex_exit(&cache->arc_stats.stat_lock,__func__);
}
inline void sarc_read_done(struct __sarc_object *obj){
	mutex_enter(&obj->obj_lock,__func__);
	if(obj->read_state == READ_STATE){
		obj->read_state = READ_FINISHED;
		cv_broadcast(&obj->cv);
	}
	mutex_exit(&obj->obj_lock,__func__);
}
int isbottom(struct __sarc_state *state,struct __sarc_object *obj){


   return 0;
}
struct __sarc_object *__sarc_lookup(struct __sarc *cache, uint64_t id,uint64_t offset){
#define IN_MFUG 1
    struct __sarc_object *obj,*obj_prev ;
    lmutex_t *hash_lock;
    uint64_t key;
    double ratio,t,LargeRatio=0;
    key = lfs_arc_hash(id,offset);
#ifdef LFS_DEBUG

	pthread_t tid=pthread_self();
	printf("lookup id=%d,off=%d tid=%u\n",(int)id,(int)offset,(unsigned int)tid);

#endif
	obj = __sarc_hash_lookup(id,offset,&hash_lock);
    if (obj && obj->read_state == READ_FINISHED ) {
	arc_stat_hit_update(cache);
/* i) hit random list 
 */
	if(obj->state == &cache->random){
		if(isbottom(obj->state,obj)){
			mutex_enter(&cache->arc_lock,__func__);
			cache->seqMiss = 0;
        /*if hit bottom of random,adapt should be lower */
			cache->adapt = -0.5;
			mutex_exit(&cache->arc_lock,__func__);
		}
		__sarc_move(cache,obj,&cache->random,0);
		mutex_exit(hash_lock,__func__);
		return obj;
	}
/* ii) hit seq list
 */
	else if(obj->state == &cache->seq){
		if(isbottom(obj->state,obj)){

			t = 2.0*cache->seqMiss*cache->tail_stat/cache->seq.size;
			if(t>LargeRatio){
				cache->adapt = 1;
			}
		}
		mutex_exit(hash_lock,__func__);
		__sarc_move(cache,obj,&cache->seq,0);
		obj_prev = __sarc_hash_lookup(id,offset-LFS_BLKSIZE,&hash_lock);
		if(obj_prev){
			assert(obj_prev->pcount>=0);
		        if(obj_prev->pcount){
				obj->pcount = MIN(cache->seqThreshold,obj_prev->pcount+1);
				}
			else
				obj->pcount = 0;
		}

	}

	}else{
/*iii)data miss
 * iii-1)obj==NULL
 *	then create item and insert it into hash table 
 * iii-2)obj!=NULL 
 * 	item existed but in READ_STATE,wait here
 */
		if(obj==NULL){
			arc_stat_update(cache);
 			obj = cache->ops->create(id,offset);
			printf("going to insert hash arc_lookup\n");
 		       	__sarc_hash_insert(cache, key, obj,hash_lock);
			cache->ops->fetch_from_disk(id,offset,obj);
			assert(obj);
			assert(obj->read_state == READ_STATE);
			sarc_read_done(obj);
			sarc_adjust_states(obj);
			return obj;
		}else if(obj!=NULL && obj->read_state == READ_STATE){
				mutex_exit(hash_lock,__func__);

				mutex_enter(&obj->obj_lock,__func__);
				printf("going to wait\n");
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
#endif
