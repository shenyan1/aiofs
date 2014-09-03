#ifndef _LFS_FREEMAP_H_
#define _LFS_FREEMAP_H_

#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<inttypes.h>
#include<sys/types.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"lfs.h"
#include"arc.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"eserver.h"
#include "lfs_define.h"
/*********freelist structure and operator*******/


struct FreeListNode
{
    uint64_t offset;
    int idx;
    struct FreeListNode *next;
};
typedef struct FreeListNode free_node_t;
typedef struct FreeListNode file_inode_t;
typedef struct FreeListNode dir_inode_t;


void InitListNode (free_node_t *, uint64_t, int);

// add a listnode after @pnode ,with @2 @3
void PushListNode (free_node_t * pnode, uint64_t, int);

// add a listnode as the list head
void InsertListNode (uint64_t, int);
/***********************************************/

// find the offset of the freemap by idx
offset_t GetFreePosByidx (int _idx);

void SetFreeMapUsed (int _idx, offset_t);

//pop the first node in the list
free_node_t *PopListNode (free_node_t * _phead);

/// Initialize the Freemap.
int LoadFreemapEntry ();

/// malloc 1M space. This space is get from the head of the freelist;
// return the offset of 1M space;
// if return 0 means Malloc failed.
offset_t Malloc_Freemap ();

int Free_Freemap (offset_t);

void freemap_test ();

#endif
