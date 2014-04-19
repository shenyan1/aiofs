#ifndef _LFS_H
#define _LFS_H
#include "avl.h"
#include "lfs_err.h"
#include "arc.h"
#include "sarc.h"
#define true 1
#define false 0
#include<unistd.h>
#include<inttypes.h>
#include<stdio.h>
#include"lfs_cache.h"
#include<assert.h>
#include<stdlib.h>
#include"config.h"
typedef struct file_entry{
	char filename[8];
	uint32_t fsize;
	uint32_t is_free;   
	uint64_t *meta_table;
}file_entry_t;

typedef struct lfs_info{
	int fd;
	uint64_t off;
	file_entry_t *f_table;
	char *block_device;
	avl_tree_t *root;
#ifdef USE_SARC
	sarc_t *sarc_cache;
#else
	arc_t *arc_cache;
#endif
	uint64_t *freemap;
	cache_t *lfs_cache;
	cache_t *lfs_obj_cache;
	uint32_t max_files;
}lfs_info_t;
extern uint64_t getphymemsize(void);

/*
 * using pthread to mutex our avl and arc operations
 */
typedef struct kmutex {
	void		*m_owner;
	uint64_t	m_magic;
	pthread_mutex_t	m_lock;
} kmutex_t;

#define FSNAME "LFS"
#define VERSION "01"
#define MAX_FILE_NO 10<<10

#define LFS_FILE_ENTRY sizeof(uint64_t)+ sizeof(uint32_t)
#define MAX_FILES (10<<10)
#define LFS_SPACE_ENTRY (LFS_FILE_ENTRY+(2*sizeof(uint32_t)+sizeof(uint64_t)+57*sizeof(uint64_t))*MAX_FILES)
#define P2ROUNDUP(x, align)	(-(-(x) & -(align)))
#define P2ALIGN(x, align)	((x) & -(align))
#define LFS_DATA_DOMAIN1  (LFS_SPACE_ENTRY+((1<<20)*2*sizeof(uint64_t)))
#define LFS_DATA_DOMAIN     P2ROUNDUP(LFS_DATA_DOMAIN1,1<<9)

#define AVG_FSIZE (200<<20)

#define FILE_ENTRYS 57

#define LFS_FREE 0

#define LFS_NFREE 1

#define MAX_FSIZE (256<<20)

#define BLKSHIFT 20
#define ASSERT assert

#define VERIFY assert

#define MIN(a, b)       ((a) < (b) ? (a) : (b))
#define MAX(a, b)       ((a) < (b) ? (b) : (a))

#define LFS_BLKSIZE (1<<20)
extern uint64_t getlocalp(uint64_t id);
extern uint64_t arc_hash_init(void);
extern void lfs_mutex(pthread_mutex_t *lock);
extern void lfs_unmutex(pthread_mutex_t *lock);
#endif
