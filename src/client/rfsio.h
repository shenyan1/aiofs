/* The method to read/write for user
 */
#ifndef __RFSIO_H
#define __RFSIO_H
//#include"../lfs.h"

#include<assert.h>
#include<inttypes.h>
#include<lfs_define.h>
#define READ_SIZE (sizeof(char)+sizeof(int)+sizeof(int)+sizeof(uint64_t)+sizeof(uint64_t)+1)
/*READ protocol: READ(1B)     |Inode(4B)  |SHMID(4B)  |offset(8B)      |size(8B)*/
int decode_readprotocol (char *pro);
int rfs_write (int id, char *buffer, uint64_t size, uint64_t offset);
int rfs_read (int id, char *buffer, uint64_t size, uint64_t offset);
char *id2protocol (int shmid);
/* rfs_return_data: return the protocol (error code or data) to client.
 * 
 */
//int rfs_return_data (char *proto, int clifd);
/* return current files in rfs.
 * fsid is reserved for future.
 */
int curmax_files (int fsid);
int stopfs();
int wait_res (int connfd);

int lsfs (char **argv);
#define P2ALIGN(x, align)	((x) & -(align))
#define ISALIGNED(v,a)      ((((intptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)
/* create rfs file.
 */
int rfs_create (char *fname);
/* return whether the file is free.
 */
int fdisfree (int fid);
/* preallocate a large file for file
 */
int rfs_fallocate (char *fname, int size);
int rfs_rmdir (char *fname);
inode_t rfs_open (char *fname);
int rfs_remove (char *fname);
int rfs_mkdir (char *dirname);
uint64_t cur_usec(void);
void _cli_printf (const char *fmt, ...);
#define LFS_OK 0
#define LFS_FAILED -1
#endif
