#define _XOPEN_SOURCE 500
#include<stdio.h>
#include<unistd.h>
#include"lfs.h"
#include"arc.h"
#include<time.h>
#include<assert.h>
#include<stdlib.h>
#include"lfs_err.h"
#include"lfs_ops.h"
#include"lfs_thread.h"
#include"lfs_cache.h"
/*
 */
extern lfs_info_t lfs_n;
/* LFS METHOD to create a file
 * param: int size
 */
//	arc_cache =  cache_create("arc_cache", 1<<20, sizeof(char*),NULL, NULL);

/**
* Here are the operations implemented
*/
uint64_t __op_hash(uint64_t key)
{
        return key;
}

struct object *getobj(struct __arc_object *e){
		return __arc_list_entry(e,struct object,entry);
}
static int __op_compare(struct __arc_object *e, uint64_t id,uint64_t offset)
{
        struct object *obj = __arc_list_entry(e, struct object, entry);
        return obj->id == id && obj->offset == offset;
}

static struct __arc_object *__op_create(uint64_t id,uint64_t offset)
{
        struct object *obj = cache_alloc(lfs_n.lfs_obj_cache);
		obj->data = cache_alloc(lfs_n.lfs_cache);
#ifdef LFS_DEBUG
		printf("\n1:cache_alloc obj=%p,id=%"PRIu64",offset=%"PRIu64"",&obj->entry,id,offset);
#endif
		if(offset < 0 || id <0 ){
			printf("offset =%"PRIu64",id=%"PRIu64",when create arc element\n",offset,id);
			exit(1);
			return NULL;
		}
		assert(id!=0);
        obj->offset = offset;
        obj->id = id;
/*
 * At this time we read from our arc buffer. if the buffer isn't in arc,we call this,so read from disk now.
 */
        //read from disk to obj->data
        __arc_object_init(&obj->entry, 1<<20);
        return &obj->entry;
}

static inline int  __op_fetch_disk(uint64_t id,uint64_t offset,struct __arc_object *e){
		
	struct object *obj=getobj(e);
	pread(lfs_n.fd,obj->data,1<<20,offset+lfs_n.f_table[id].meta_table[0]);
	return 1;
}
static int __op_fetch(struct __arc_object *e)
{
		int ret;
		uint64_t offset,id;
        struct object *obj = __arc_list_entry(e, struct object, entry);
		obj->data = cache_alloc(lfs_n.lfs_cache);

		offset = obj->offset;
		id = obj->id;
#ifdef LFS_DEBUG
		printf("\n2:cache_alloc obj=%p,id=%"PRIu64",offset=%"PRIu64"",&obj->entry,id,offset);
#endif
		ret = pread(lfs_n.fd,obj->data,1<<20,offset+lfs_n.f_table[id].meta_table[0]);
		if(ret != 1<<20)
			return 1;
		
        return 0;
}

static void __op_evict(struct __arc_object *e)
{
        struct object *obj = __arc_list_entry(e, struct object, entry);
        cache_free(lfs_n.lfs_cache,obj->data);
}

static void __op_destroy(struct __arc_object *e)
{
        struct object *obj = __arc_list_entry(e, struct object, entry);
//		free(obj->data);
		mutex_destroy(&e->obj_lock);
//		printf("obj's offset=%"PRIu64"id=%"PRIu64"",obj->offset,obj->id);
		cv_destroy(&e->cv);
		if(e->state !=&lfs_n.arc_cache->mrug && e->state != &lfs_n.arc_cache->mfug)
			cache_free(lfs_n.lfs_cache,obj->data);
       // free(obj);
		cache_free(lfs_n.lfs_obj_cache,obj);
}

static struct __arc_ops ops = {
        .hash            = __op_hash,
        .cmp             = __op_compare,
        .create          = __op_create,
        .fetch           = __op_fetch,
        .evict           = __op_evict,
		.fetch_from_disk = __op_fetch_disk,
        .destroy         = __op_destroy
};

int file_create(int size){
	int i;
	char filename[8]={"LFS_T"};
	uint64_t tmp_off;
	int fsize_phy,is_free_phy;
	if(size > MAX_FSIZE)
		return -LFS_EBIG;
	if(size < AVG_FSIZE/2){
		printf("the buffer allocate less than 200M\n");
	}

    for(i=0;i<lfs_n.max_files;i++){
		if(lfs_n.f_table[i].is_free==LFS_FREE){
			fsize_phy = lfs_n.f_table[i].fsize = size;	
			is_free_phy = lfs_n.f_table[i].is_free=LFS_NFREE;
			tmp_off = getlocalp(i)-16;

			printf("id =%d is available\n,tmpoff=%"PRIu64"",i,tmp_off);
			pwrite(lfs_n.fd,filename,sizeof(uint64_t),tmp_off);
			tmp_off += sizeof(uint64_t);
			pwrite(lfs_n.fd,&fsize_phy,sizeof(uint32_t),tmp_off);
			tmp_off += sizeof(uint32_t);
			pwrite(lfs_n.fd,&is_free_phy,sizeof(uint32_t),tmp_off);
			return i;	
		}
	}
    return -LFS_ENOSPC;
}
inline uint64_t getdiskpos(uint64_t offset){
	uint64_t off;
	off = offset / LFS_BLKSIZE;
	off = off * LFS_BLKSIZE;

	assert(off<=offset);
	return off;
}
inline uint64_t getdiskrpos(uint64_t offset){

	if(offset % LFS_BLKSIZE ==0){
		return getdiskpos(offset);
	}
	return getdiskpos(offset) + (1<<20);

}
/* LFS METHOD to write a file
 * param:int  id
 * param:char *buffer
 * param:int  size
 * param:uint64_t offset
 * return 0 means OK
 */
int file_write(int id,char *buffer,uint64_t size,uint64_t offset){
	int ret;
	file_entry_t entry;
	entry = lfs_n.f_table[id];
	/* compute the blockid  in file id,offset / 1M;
     */
	if(lfs_n.f_table[id].is_free==LFS_FREE){
		printf("going to write a free dnode,id=%d\n",id);
		return -LFS_EPM;
	}
	if(size > AVG_FSIZE){
		printf("it will write larger than 200M\n");
		return true;
	}
	if(offset + size > entry.fsize){
		printf("write err,id=%d,write beyond filesize,filesize=%d,write %"PRIu64"\n",id,entry.fsize,offset+size);
		return -LFS_EBOUND;
	}
	/* in the write method, so we can just write the buffer with the offset and size.
     */
	
	ret =pwrite(lfs_n.fd,buffer,size,offset+lfs_n.f_table[id].meta_table[0]);
	if(ret == -1){
		printf("pwrite error\n");
		return true;
	}
	return false;

}

int file_open(int id,int flag){
	if(lfs_n.fd == -1){
		printf("the fs is crashed\n");
		return -LFS_EOPEN;
	}
	if(lfs_n.f_table[id].is_free == LFS_FREE){
		printf("the file %d is freed when open\n",id);
		return -LFS_EOPEN;
	}
	return LFS_SUCCESS;
}

int file_remove(int id){
	int size;
	uint64_t fp;
	char *buffer;
	size = (FILE_ENTRYS-1)*sizeof(uint64_t);
	buffer = malloc(size);
	memset(buffer,0,size);
	if(lfs_n.f_table[id].is_free == LFS_FREE){
		printf("the file %d is already removed\n",id);
		return false;
	}
	if(lfs_n.f_table[id].meta_table[1] != 0){
		//TO DO:put the meta_table's content into small space_map.
	}
	lfs_n.f_table[id].is_free = LFS_FREE;
	fp = getlocalp(id);
	pwrite(lfs_n.fd,buffer,size,fp+sizeof(uint64_t));
	return true;
	
}

/* LFS METHOD to read a file
 * param:int   id
 * param:char *buffer
 * param:int   size
 * param:uint64_t offset
 * +++++++++++++++++++++++++++++++++++++++++++++++
 * |l_off       offset     |(r_off) 			 |
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
int file_read(int id,char *buffer,uint32_t size,uint64_t offset){
	
	uint64_t l_off,r_off;
	int nblks;
	int i,bufoff,tocpy;
	struct object *obj;
 	struct __arc_object *entry;
	l_off = getdiskpos(offset);
	r_off = getdiskpos(offset + size);
	if(buffer==NULL){
		printf("the buffer shouldn't be NULL\n");
		return -1;
	}
	if(lfs_n.f_table[id].is_free == LFS_FREE){
		printf("read refused id=%d\n",id);
		return -LFS_EPM;
	}
	if(l_off == r_off){
		/* read from arc,by 1 MetaBytes.
		 */
#ifdef LFS_DEBUG
			printf("going to arc_lookup,id=%d,off=%"PRIu64"\n",id,l_off);
#endif
			entry = __arc_lookup(lfs_n.arc_cache,id,l_off);
			obj = __arc_list_entry(entry, struct object, entry);
			memcpy(buffer,obj->data + (offset - l_off),size);
			return 0;
	}
	/* the block is across only 1 block.
     */
	nblks = (getdiskrpos(offset + size) - getdiskpos(offset)) >> 20;
	bufoff = offset - l_off;
	tocpy  = LFS_BLKSIZE - offset +bufoff;
	for(i=0;i<nblks;i++){
			entry = __arc_lookup(lfs_n.arc_cache,id,l_off);
			obj = __arc_list_entry(entry, struct object, entry);
			bufoff = offset - obj->offset;
			tocpy = (int)MIN(LFS_BLKSIZE - bufoff,size);
			memcpy(buffer,obj->data + bufoff,tocpy);
			offset += tocpy;
			size -= tocpy;
			buffer = buffer + tocpy;
			l_off += LFS_BLKSIZE;
	}

	return 0;
}

void lfs_arc_init(uint64_t arc_size){
	lfs_n.arc_cache = __arc_create(&ops,arc_size); 
	if(lfs_n.arc_cache == NULL){
		printf("lfs arc create failed\n");
		exit(1);
	}
		
}


