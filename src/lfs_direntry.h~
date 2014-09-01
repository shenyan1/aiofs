#ifndef _LFS_DIRENTRY_H_
#define _LFS_DIRENTRY_H_

#include "lfs_freemap.h"
#include "lfs_dir.h"

//typedef uint32_t inode_t;
/*
#pragma pack(push)
#pragma pack(1)

typedef struct DirEntry{
	uint32_t inode_;
	char filetype_;
	uint32_t nextlen_;
	uint16_t len_;
	char * pname_;	
}dir_entry_t;

#pragma pack(pop)
*/

dir_entry_t *LoadDirEntry (offset_t);

int WriteDirEntry (dir_entry_t *, offset_t);

int WriteDirEntryNext (uint32_t, offset_t);

dir_entry_t *GetNextDirEntry (dir_entry_t *);

int IsExistDirEntry (dir_entry_t *);

void _LoadDirEntry (dir_entry_t ** ppentry, offset_t _off);
dir_entry_t *InitDirEntry (inode_t, char, uint32_t, const char *);


/*************************************/
#endif
