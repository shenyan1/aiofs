#ifndef _LFS_DIR_DEFINE_H_
#define _LFS_DIR_DEFINE_H_

//#define __DEBUG__
#ifdef __DEBUG__
#define DEBUG_FUNC1 printf("@%s:%d\n",  __FILE__, __LINE__)
#define DEBUG_FUNC(x) printf("@%s:%d!%s %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, (x));
#define DEBUGER(format,...) printf("@"__FILE__",%03d %s: "format"\n", __LINE__, __func__, ##__VA_ARGS__)
#define SYS_OUT(format,...) printf("$:"__FILE__",%03d %s: "format"\n", __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUGER(format, ...)
#define DEBUG_FUNC1
#define DEBUG_FUNC(x)
#define SYS_OUT(format,...) printf("$: "format"\n", ##__VA_ARGS__)
#endif

#define ALERTER(format,...) printf("E:@"__FILE__",%03d %s: "format"\n", __LINE__, __func__, ##__VA_ARGS__)

#define LFS_LOG(format,...) printf("@"__FILE__",%03d %s: "format"\n", __LINE__, __func__, ##__VA_ARGS__)


#define LFS_FILE_ENTRY (LFS_DIR_ENTRY + DIR_META_SIZE*MAX_DIR_NO)
#define MAX_DIR_NO 100000
#define DIR_META_SIZE (4+4+3*8+8+8*5)
#define DIR_META_BLKPTR_OFF (4 + 4 + 8)
/* |FS_METADATA|DIR_META_ENTRY|FILE_META_ENTRY|Freebitmap|
 * FILE_METASIZE : 3*8: attribute 8: filesize 5: extentInode 57*8:blkptrs
 */
#define METASIZE_PERFILE (8*3+8+5+57*8)
#define LFS_FREEBITMAP_ENTRY (LFS_FILE_ENTRY+METASIZE_PERFILE*MAX_FILES)
#define LFS_FREEMAP_ENTRY (LFS_FREEBITMAP_ENTRY + FREEBITMAP_SIZE)
#define FREEBITMAP_SIZE ((1<<(20-3))*2)
#define FREEMAP_SIZE (16<<20)
#define LFS_DIR_ENTRY (sizeof(uint64_t)+ sizeof(uint32_t))
#define LFS_DIR_META_POS(x) (LFS_DIR_ENTRY + (x)*DIR_META_SIZE)

// who know the sucks 
#define LFS_DIR_DOMAIN 100000

#define DCACHEMODE 1
#define O_DIRECT_MODE


typedef int inode_t;
typedef uint64_t offset_t;
#define LFS_SUCCESS  0
#define LFS_EBIG     1
#define LFS_EBOUND   2
#define LFS_EOPEN    3
#define LFS_ENOSPC   4
#define LFS_EPM	     5
#define LFS_INVALID  6
#define LFS_EINCACHE 7
#define LFS_PENDING  8
#define LFS_CMISS    9


/* operation commands are as followed.
 */

#define WRITE_COMMAND    1
#define MKDIR_COMMAND    2
#define GETFILES_COMMAND 3
#define ISFREE_COMMAND   4
#define STOP_FS 	 5
#define READ_COMMAND     6
#define RMDIR_COMMAND 7
#define DIR2INODE_COMMAND 8
#define LIST_COMMAND 9
#define FALLOCATE_COMMAND 10
#define MKFILE_COMMAND    11
#define RMFILE_COMMAND    12
#define FOPEN_COMMAND DIR2INODE_COMMAND

#define LFS_FAILED -1
#define LFS_OK 0

//#define LFS_DAEMON 0
#define LFS_LOG_FNAME "/tmp/rfs_logs"

#define _LFS_DEBUG 1

#define _LFS_DAEMON 1
/*

inline void DEBUG_FUNC(const char * _info){
	printf("%s @%s:%d\n", _info, __FILE__, __LINE__);
}
*/
#define FREELIST_LOCK lfs_n.cq_info.cqi_freelist_lock;
#define RFS_AIOQ    lfs_n.aioq
#define RFS_RQ      lfs_n.req_queue
#define IOCBQ_MUTEX lfs_n.cq_info.iocb_queue_mutex
#define IOCBQUEUE lfs_n.cq_info.iocbq
#define AIO_INFO lfs_n.cq_info
#define FSNAME "LFS"
#define VERSION "01"
#define MAX_FILE_NO (10<<10)

#define MAX_FILES (10<<10)
#define LFS_SPACE_ENTRY LFS_FREEMAP_ENTRY
#define LFS_DIR_INDEX_BITMAP (LFS_FREEMAP_ENTRY + (1<<20)* 2 * sizeof(uint64_t))
//(LFS_FILE_ENTRY+(2*sizeof(uint32_t)+sizeof(uint64_t)+57*sizeof(uint64_t))*MAX_FILES)

#define P2ROUNDUP(x, align)	(-(-(x) & -(align)))
#define P2ALIGN(x, align)	((x) & -(align))
#define AVG_FSIZE (200<<20)
#define MAX_FSIZE (256<<20)
#define LFS_INDEX_BLOCK_SIZE (4<<10)

#define LFS_INDEXMAP_SIZE (25 * (1<<9))
#define LFS_DATA_DOMAIN1  ( LFS_DIR_INDEX_BITMAP + LFS_INDEXMAP_SIZE )
#define LFS_DIR_INDEX_ENTRY P2ROUNDUP(LFS_DATA_DOMAIN1,1<<9)
#define LFS_DATA_DOMAIN  (LFS_DIR_INDEX_ENTRY + 2*AVG_FSIZE)
#define LFS_FINODE_START 100000



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
/* rfs_return_data: return the protocol (error code or data) to client.
 * 
 */

#endif
