#ifndef __LFS_SYS_H
#define __LFS_SYS_H
#include<inttypes.h>
//#include"lfs_dir.h"
#include"lfs.h"
uint64_t getlocalp (uint64_t id);
uint64_t cur_usec (void);
uint64_t getphymemsize ();
int buf2id (char *ptr);
void lfs_printf (const char *fmt, ...);
void lfs_printf_debug (const char *fmt, ...);
typedef uint64_t u64;
u64 lfsgetblk (lfs_info_t * plfs, inode_t inode, u64 offset);
#endif
