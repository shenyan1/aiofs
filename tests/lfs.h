#ifndef _LFS_H
#define _LFS_H
#include<unistd.h>
#include<inttypes.h>
typedef enum boolean { B_FALSE, B_TRUE } boolean_t;

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
}lfs_info_t;
#define FSNAME "LFS"
#define VERSION "01"
#define MAX_FILE_NO 10<<10

uint64_t getphymemsize(){
	return sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
}

/*
 * using pthread to mutex our avl and arc operations
 */
typedef struct kmutex {
	void		*m_owner;
	uint64_t	m_magic;
} kmutex_t;

#define LFS_FILE_ENTRY sizeof(uint64_t)

#define LFS_SPACE_ENTRY (LFS_FILE_ENTRY+(2*sizeof(uint32_t)+sizeof(uint64_t)+57*sizeof(uint64_t))*(10<<10))

#define LFS_DATA_DOMAIN  (LFS_SPACE_ENTRY+((1<<20)*2*sizeof(uint64_t)))

#define AVG_FSIZE (200<<20)

#define FILE_ENTRYS 57

#define LFS_FREE 0

#define LFS_NFREE 1

#define MAX_FSIZE (256<<20)

#define ASSERT assert

#define VERIFY assert

#define LFS_BLKSIZE (1<<20)
extern uint64_t getlocalp(uint64_t id);
extern uint64_t arc_hash_init(void);
#endif
