#ifndef _LFS_DIR_H_
#define _LFS_DIR_H_

#include "lfs_freemap.h"

#pragma pack(push)
#pragma pack(1)
typedef struct DirMetaData
{
    uint32_t cur_size_;
    uint32_t Ext_mask_;
    offset_t idx_blkptr_;
    offset_t s_blkptr_[5];
    uint64_t attribute[3];
} dir_meta_t;

//#define ENTRY_DATA_SIZE (sizeof(uint32_t) + sizeof(char) +sizeof(uint32_t) + sizeof(uint16_t))
struct DirEntry
{
    inode_t inode_;
    char filetype_;
    uint32_t nextoff_;
    uint16_t len_;
    char *pname_;
} __attribute__ ((__packed__));

typedef struct DirEntry dir_entry_t;
#pragma pack(pop)

int IsDirorFile (inode_t inode);

int _lfs_pread (int fd, void *_ptr, size_t _size, offset_t _pos);


int _lfs_pwrite (int fd, void *_ptr, size_t _size, offset_t _pos);
/*************************************/
//loaddirmeta and initialize the inodelist
int LoadDirMeta ();

int WriteDirMeta (inode_t);
/*************************************/

//we define all 5 s_blkptr_ is zero, it should be a idle dir. 
int IsIdleDirInode (offset_t *);

/*************************************/
/// return the new dir inode,
//0 represent make fail, maybe an error dir name
//
inode_t MakeDir (const char *);

int CreateFile (const char *);

int AddDirEntryFile (inode_t, const char *);

/* print all things in this dir
 * @1  dir name eg. /aaa/aa/a/
 */
int PrintDir (const char *);

inode_t Open (const char *);

//find the subdir in the dir presenting by inode 
inode_t FindSubdir (inode_t, const char *);

//inode_t  

inode_t Dir2Inode (const char *_pdir);


offset_t Inode2dir_meta (inode_t);

//according to the sblkptr, get the first entry offset in this block.
offset_t GetDirEntryHead (offset_t);

void SetDirEntryHead (offset_t, offset_t);

int EnableSize (offset_t, offset_t, int, int);

/// according to the offset of one sblock representing the first entry,
//traverse all the dir entry and get offset to add a entry;
//then if make it return inode>0 and vice versa.
//and then add the index in dir[inode]
int AddDirEntry (inode_t, const char *);
//AddDirEntry
/*************************************/

inode_t MallocDirInode ();

int RemoveDir ();

void dir_test (char *_pname);

#endif
