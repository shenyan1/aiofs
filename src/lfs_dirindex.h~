#ifndef _LFS_DIRINDEX_H_
#define _LFS_DIRINDEX_H_

#include "lfs_freemap.h"
#include "lfs_dir.h"
#include "lfs.h"

#define LFS_INDEX_DOMAIN 4*(1<<10)
#define LFS_BLOCK_NO 500
#define HASH_MOD 0x7FFFFFFF

typedef uint32_t hash_t;
/*
#define IDXLIST lfs_n.pindexlist
#define IDXBLK IDXLIST[_inode]
#define IDXLEAF IDXBLK[i].leaf_
#define IDXNEXTLEAF IDXBLK[i+1].leaf_
#define DIRMETA_ARRAY lfs_n.pdirmarray 
#define IDXMAP lfs_n.pindexmap
*/
#pragma pack(push)
#pragma pack(1)
typedef struct DirIndexLeaf
{
    hash_t hash_;
    uint32_t offset_;
} dir_index_leaf_t;


typedef struct DirIndexEntry
{
    hash_t hash_;
    offset_t off_;
    dir_index_leaf_t *leaf_;
} dir_index_t;

#pragma pack(pop)

hash_t RSHash (char *str);


int InitDirTree ();
/* Load the Index of inode_t-dir from indexblk offset_t
 * @1 dir inode
 * @2 the pos of the index, dir.idx_blkptr
 */
int LoadDirIndex (inode_t /*, offset_t */ );

/*Load the leafblock
 * @ idxblkptr
 * RETURN
 */
int LoadLeafBlocks (dir_index_t *);

int WriteDirIndex (offset_t, hash_t, uint32_t);

offset_t GetIndexDiskPos (inode_t, int, int);

/******************************************/

offset_t MallocIndexSpace ();

/******************************************/
/* initialize an index and retutn the offset 
 offset is serving for dir-index_blkptr
 */
offset_t InitDirIndex (inode_t inode);
dir_index_t *InitIndexBlock (offset_t, hash_t, offset_t, dir_index_leaf_t *);

int ChangeLeafBLK (dir_index_leaf_t *, offset_t, hash_t, uint32_t);

int ChangeIndexBLK (dir_index_t *, offset_t, hash_t, uint32_t);


int LoadIndexBitMap ();

/* 
 * insert an entry in _inode dir_index  
 * @1 dir inode
 * @2 name of the inserted dir|file
 * @3 pos of the inserted dir|file in dir-file
 * RETURN to be determined
*/
int InsertIndex (inode_t _inode, const char *pname, offset_t _off);
/*
 * According the pos to 
 */


inode_t IsFindTarget (offset_t, char *);

/*
 * find the pos of the dir-entry by index
 * @1 dir inode
 * @2 name of the finded dir|file
 * RETRUN  inode  0 representing find failed.
 */
inode_t FindIndex (inode_t _inode, char *pname);
offset_t FindIndexOff (inode_t _inode, char *_pname);

int DeleteIndex (inode_t inode, char *pname);

dir_entry_t *LoadDirEntry (offset_t _off);
int Print_Index (inode_t _inode);
void test_index ();

#endif
