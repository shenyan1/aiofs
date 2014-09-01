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
//#define _LFS_DEBUG 1
/*

inline void DEBUG_FUNC(const char * _info){
	printf("%s @%s:%d\n", _info, __FILE__, __LINE__);
}
*/
#endif