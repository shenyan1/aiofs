#ifndef __LFS_FOPS_H
#define __LFS_FOPS_H

#include<stdio.h>
#include<unistd.h>
#include"lfs.h"
#include"arc.h"
#include"sarc.h"
#include<time.h>
#include<assert.h>
#include<stdlib.h>
#include"lfs_err.h"
#include"lfs_ops.h"
#include"lfs_thread.h"
#include"config.h"
#include"lfs_cache.h"

extern int file_create (int size);
extern int getfilemetadata (int id);
extern int getfilesize (int id);
extern int file_remove (int id);
extern inline uint64_t getdiskpos (uint64_t offset);
extern inline uint64_t getdiskrpos (uint64_t offset);
/* LFS METHOD to write a file
 * param:int  id
 * param:char *buffer
 * param:int  size
 * param:uint64_t offset
 * return 0 means OK
 */
extern int file_write (int id, char *buffer, uint64_t size, uint64_t offset);
extern int file_open (int id, int flag);

/* LFS METHOD to read a file
 * param:int   id
 * param:char *buffer
 * param:int   size
 * param:uint64_t offset
 * +++++++++++++++++++++++++++++++++++++++++++++++
 * |l_off       offset     |(r_off)                      |
 * +++++++++++++++++++++++++++++++++++++++++++++++
 */
extern int file_read (int id, char *buffer, uint32_t size, uint64_t offset);

extern struct __sarc_ops sarc_ops;
extern void lfs_arc_init (uint64_t arc_size);
#endif
