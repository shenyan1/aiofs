#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
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
#include"lfs_test.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"eserver.h"
#include "lfs_define.h"
#include "lfs_direntry.h"
#include "lfs_dirindex.h"
#include "lfs_sys.h"
extern lfs_info_t lfs_n;

#define IDXLIST lfs_n.pindexlist
#define IDXBLK IDXLIST[_inode]
#define IDXLEAF IDXBLK[i].leaf_
#define IDXNEXTLEAF IDXBLK[i+1].leaf_
#define DIRMETA_ARRAY lfs_n.pdirmarray
#define IDXMAP lfs_n.pindexmap
//#define false 0
//#define true 1

hash_t RSHash (char *str)
{
    hash_t b = 378551;
    hash_t a = 63569, hash = 0;
    while (*str)
      {
	  hash = hash * a + (*str++);
	  a *= b;
      }
    return (hash & HASH_MOD);
}

/******************************/

int BinaryBit (char x)
{
    int iRet = 0;
    if ((x & 0xF0) == 0)
	iRet += 4;
    else
	x >>= 4;

    if ((x & 0xC) == 0)
	iRet += 2;
    else
	x >>= 2;

    if ((x & 0x2) == 0)
	iRet += 1;
    return iRet;
}

int First1 (char x)
{
    int i = 0, bit = 1;
    for (i = 0, bit = 1; i < 8; ++i, bit <<= 1)
      {
	  if (bit & x)
	      return i;
      }
    return -1;
}

offset_t MallocIndexSpace ()
{
    int i = 0, j = 0;
    offset_t pos = 0;
    for (i = 0; i < LFS_INDEXMAP_SIZE / 8; ++i)
      {
	  j = First1 (IDXMAP[i]);
	  if (j != -1)
	    {
		char mask = ~(1 << j);
		char a = IDXMAP[i] & mask;
		IDXMAP[i] = a;

		_lfs_pwrite (lfs_n.fd, &a, 1, LFS_DIR_INDEX_BITMAP + i);

#ifdef O_DIRECT_MODE
		_lfs_pread (lfs_n.fd, &a, 1, LFS_DIR_INDEX_BITMAP + i);
#else
		pread (lfs_n.fd, &a, 1, LFS_DIR_INDEX_BITMAP + i);
#endif

		pos =
		    (i * 8 + j) * LFS_INDEX_BLOCK_SIZE + LFS_DIR_INDEX_ENTRY;
		DEBUGER ("b%X %X %X %d %d %llu", IDXMAP[i], a, mask, i, j,
			 pos);
		char *buffer = malloc (LFS_INDEX_BLOCK_SIZE);
		memset (buffer, 0, sizeof (LFS_INDEX_BLOCK_SIZE));
		_lfs_pwrite (lfs_n.fd, buffer, LFS_INDEX_BLOCK_SIZE, pos);
		free (buffer);
		return pos;
	    }
      }
    return pos;
}

int LoadIndexBitMap ()
{
    IDXMAP = malloc (LFS_INDEXMAP_SIZE);
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, IDXMAP, LFS_INDEXMAP_SIZE, LFS_DIR_INDEX_BITMAP);
#else
    pread (lfs_n.fd, IDXMAP, LFS_INDEXMAP_SIZE, LFS_DIR_INDEX_BITMAP);
#endif
    return 0;
}

int InitDirTree ()
{
    DEBUG_FUNC ("!!!");
    LoadIndexBitMap ();
    IDXLIST = malloc (sizeof (dir_index_t *) * MAX_FILE_NO);
    memset (IDXLIST, 0, sizeof (dir_index_t *) * MAX_FILE_NO);
    return 0;
}

/******************************/
int LoadDirIndex (inode_t _inode /*, offset_t _off */ )
{

    int i = 0;
    offset_t off = DIRMETA_ARRAY[_inode].idx_blkptr_;
    dir_index_leaf_t *ptmp = malloc (LFS_INDEX_DOMAIN);
    IDXLIST[_inode] = malloc (sizeof (dir_index_t) * LFS_BLOCK_NO);
    memset (IDXLIST[_inode], 0, sizeof (dir_index_t) * LFS_BLOCK_NO);
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, ptmp, LFS_INDEX_DOMAIN, off);
#else
    pread (lfs_n.fd, ptmp, LFS_INDEX_DOMAIN, off);
#endif
    DEBUGER ("%d off = %llu, ", _inode, off);

    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  IDXLIST[_inode][i].hash_ = ptmp[i].hash_;
	  IDXLIST[_inode][i].off_ = ptmp[i].offset_;
	  DEBUGER ("%lu, %lu", ptmp[i].offset_, ptmp[i].hash_);
	  if (ptmp[i].offset_ != 0)
	    {
		IDXLIST[_inode][i].leaf_ = malloc (LFS_INDEX_DOMAIN);
#ifdef O_DIRECT_MODE
		_lfs_pread (lfs_n.fd, IDXLIST[_inode][i].leaf_,
			    LFS_INDEX_DOMAIN, (offset_t) ptmp[i].offset_);
#else
		pread (lfs_n.fd, IDXLIST[_inode][i].leaf_, LFS_INDEX_DOMAIN,
		       (offset_t) ptmp[i].offset_);
#endif
	    }
	  else
	      break;
      }
    return 1;
}

int LoadLeafBlocks (dir_index_t * _idxblk)
{
    if (_idxblk->leaf_ == NULL)
      {
	  _idxblk->leaf_ = malloc (LFS_INDEX_DOMAIN);
#ifdef O_DIRECT_MODE
	  _lfs_pread (lfs_n.fd, _idxblk->leaf_, LFS_INDEX_DOMAIN,
		      (offset_t) _idxblk->off_);
#else
	  pread (lfs_n.fd, _idxblk->leaf_, LFS_INDEX_DOMAIN,
		 (offset_t) _idxblk->off_);
#endif
      }
    return 0;
}

offset_t GetIndexDiskPos (inode_t _inode, int _i, int _j)
{
    offset_t iRet = 0;
    if (_j < 0)
      {
	  iRet =
	      DIRMETA_ARRAY[_inode].idx_blkptr_ +
	      _i * sizeof (dir_index_leaf_t);
      }
    else
	iRet = IDXBLK[_i].off_ + _j * sizeof (dir_index_leaf_t);
    DEBUGER ("Pos=%llu inode %d i=%d j=%d %llu", iRet, _inode, _i, _j,
	     DIRMETA_ARRAY[_inode].idx_blkptr_);
    return iRet;
}

int WriteDirIndex (offset_t _pos, hash_t _hash, uint32_t _off)
{
    DEBUGER ("%llu %lu %lu", _pos, _hash, _off);
    _lfs_pwrite (lfs_n.fd, &_hash, sizeof (hash_t), _pos);
    _lfs_pwrite (lfs_n.fd, &_off, sizeof (uint32_t), _pos + sizeof (hash_t));
    return 0;
}

/******************************/
dir_index_t *InitIndexBlock (offset_t _pos, hash_t _hash, offset_t _off,
			     dir_index_leaf_t * _pleaf)
{
    dir_index_t *pRet = malloc (LFS_INDEX_DOMAIN);
    WriteDirIndex (_pos, HASH_MOD, 0);
    pRet[0].hash_ = _hash;
    pRet[0].off_ = _off;
    pRet[0].leaf_ = _pleaf;
    return pRet;
}

offset_t InitDirIndex (inode_t _inode)
{
    DEBUGER ("init %d", _inode);
    offset_t off = MallocIndexSpace ();
    if (off == 0)
      {
	  ALERTER ("");
      }
    IDXBLK = InitIndexBlock (off, HASH_MOD, 0, NULL);
    DIRMETA_ARRAY[_inode].idx_blkptr_ = off;
    WriteDirMeta (_inode);
    return 0;
}

offset_t InitIndexLeaf (inode_t _inode, int i)
{
    offset_t off = MallocIndexSpace ();
    DEBUGER ("%llu", off);
    ChangeIndexBLK (IDXBLK + i, DIRMETA_ARRAY[_inode].idx_blkptr_,
		    IDXBLK[i].hash_, off);
    IDXBLK[i].leaf_ = malloc (LFS_INDEX_DOMAIN);
    memset (IDXBLK[i].leaf_, 0, LFS_INDEX_DOMAIN);
    return 0;
}

int ChangeLeafBLK (dir_index_leaf_t * _leaf, offset_t _pos, hash_t _hash,
		   uint32_t _off)
{
    _leaf->hash_ = _hash;
    _leaf->offset_ = _off;
    WriteDirIndex (_pos, _hash, _off);
    return 0;
}

int ChangeIndexBLK (dir_index_t * _pidx, offset_t _pos, hash_t _hash,
		    uint32_t _off)
{
    _pidx->hash_ = _hash;
    _pidx->off_ = _off;
    WriteDirIndex (_pos, _hash, _off);
    DEBUGER ("pos%llu hash%lu off%lu", _pos, _hash, _off);
    return 0;
}

int InsertIndex (inode_t _inode, const char *_pname, offset_t _off)
{
    hash_t hash = RSHash (_pname);
    int i = 0, j = 0, k = 0;
    int cnt = 0;
    uint32_t off = (uint32_t) _off;	// _off is less than 2G 
    DEBUGER ("%lu %d %s %llu ", hash, _inode, _pname, _off);

    if (IDXBLK == NULL)
      {
	  DEBUGER ("");
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		InitDirIndex (_inode);
	    }
	  LoadDirIndex (_inode);
      }

    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  if (IDXBLK[i].hash_ >= hash)
	    {			// i is the target-pos
		int notfull = false;
		if (IDXBLK[i].off_ == 0)
		  {
		      InitIndexLeaf (_inode, i);
		  }
		if (IDXLEAF == NULL)
		    LoadLeafBlocks (IDXBLK + i);

		for (j = 0; j < LFS_BLOCK_NO; ++j)
		  {
		      if (IDXLEAF[j].offset_ == 0 && IDXLEAF[j].hash_ == 0)
			{	// is empty
			    IDXLEAF[j].hash_ = hash;
			    IDXLEAF[j].offset_ = off;
			    offset_t pos = GetIndexDiskPos (_inode, i, j);
			    WriteDirIndex (pos, hash, off);
			    notfull = true;
			    break;
			}
		  }
		if (notfull == false)
		  {		/// no update disk
		      // malloc new leaf,
		      //  IDXBLX shift right 
		      offset_t new_off = MallocIndexSpace ();

		      for (j = LFS_BLOCK_NO - 1; j >= i + 1; --j)
			{
			    //IDXBLK[j].hash_ = IDXBLK[j - 1].hash_;
			    //IDXBLK[j].off_ = IDXBLK[j - 1].off_;
			    IDXBLK[j].leaf_ = IDXBLK[j - 1].leaf_;
			    ChangeIndexBLK (IDXBLK + j,
					    GetIndexDiskPos (_inode, j, -1),
					    IDXBLK[j - 1].hash_,
					    IDXBLK[j - 1].off_);
			}
		      hash_t hash_inserted =
			  (hash > IDXLEAF[0].hash_ ? IDXLEAF[0].hash_ : hash);
		      uint32_t off_inserted = (uint32_t) new_off;
		      ChangeIndexBLK (IDXBLK + i,
				      GetIndexDiskPos (_inode, i, -1),
				      hash_inserted, IDXBLK[i].off_);
		      ChangeIndexBLK (IDXBLK + i + 1,
				      GetIndexDiskPos (_inode, i + 1, -1),
				      IDXBLK[i + 1].hash_, off_inserted);
		      IDXBLK[i + 1].leaf_ = malloc (LFS_INDEX_DOMAIN);
		      memset (IDXBLK[i + 1].leaf_, 0, LFS_INDEX_DOMAIN);

		      cnt = 0;
		      for (j = 0, k = 0; j < LFS_BLOCK_NO; ++j)
			{
			    if (IDXLEAF[j].hash_ > IDXBLK[i].hash_)
			      {
				  cnt++;
				  ChangeLeafBLK (IDXNEXTLEAF + k,
						 GetIndexDiskPos (_inode,
								  i + 1, k),
						 IDXLEAF[j].hash_,
						 IDXLEAF[j].offset_);
				  ChangeLeafBLK (IDXLEAF + j,
						 GetIndexDiskPos (_inode, i,
								  j), 0, 0);
				  ++k;
			      }
			}
		      DEBUGER ("%d leafs changed", cnt);
		      if (hash > IDXBLK[i].hash_)
			{	//inserted index
			    ChangeLeafBLK (IDXNEXTLEAF + k,
					   GetIndexDiskPos (_inode, i + 1, k),
					   hash, off);
			}
		      else
			{
			    for (j = 0; j < LFS_BLOCK_NO; ++j)
			      {
				  if (IDXLEAF[j].offset_ == 0)
				    {
					offset_t pos =
					    GetIndexDiskPos (_inode, i, j);
					ChangeLeafBLK (IDXLEAF + j, pos, hash,
						       off);
					break;
				    }
			      }
			}
		  }
		break;
	    }
      }
    return 1;
}

inode_t IsFindTarget (offset_t _off, char *_pname)
{
    /// LoadDirEntry  get the filename
    dir_entry_t *pdir;
    inode_t inode;
    _LoadDirEntry (&pdir, _off);
    DEBUGER ("STRCMP %s %s", pdir->pname_, _pname);
    lfs_printf ("pdir=%p\n", pdir);
    if (strcmp (pdir->pname_, _pname) == 0)
      {
	  inode = pdir->inode_;
	  free (pdir);
	  return inode;
      }
    free (pdir);
    return 1;
}

#if 0
inode_t IsFindTarget (offset_t _off, char *_pname)
{
    /// LoadDirEntry  get the filename

    _LoadDirEntry (&pdir, offset_t _off)
	dir_entry_t *pdir = LoadDirEntry (_off);
    DEBUGER ("STRCMP %s %s", pdir->pname_, _pname);
    lfs_printf ("pdir=%p\n", pdir);
    if (strcmp (pdir->pname_, _pname) == 0)
      {
	  free (pdir);
	  return pdir->inode_;
      }
    free (pdir);
    return 1;
}
#endif

inode_t FindIndex (inode_t _inode, char *_pname)
{
    hash_t hash = RSHash (_pname);
    int i, j;
    DEBUGER ("%d %s", _inode, _pname);
    if (IDXBLK == NULL)
      {
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		return 0;	// no entry in the dir
	    }
	  LoadDirIndex (_inode);
      }
    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  if (IDXBLK[i].hash_ >= hash)
	    {
		if (IDXLEAF == NULL)
		  {
		      LoadLeafBlocks (IDXBLK + i);
		  }
		for (j = 0; j < LFS_BLOCK_NO; ++j)
		  {
		      if (IDXLEAF[j].hash_ == hash)
			{

			    inode_t inode =
				IsFindTarget (IDXLEAF[j].offset_, _pname);
			    if (inode > 0)
			      {
				  DEBUGER ("FindIt!%d", inode);
				  return inode;
			      }
			}
		  }
		break;
	    }
      }
    DEBUGER ("Find No Index %s %d", _pname, _inode);
    return 0;
}

offset_t FindIndexOff (inode_t _inode, char *_pname)
{
    hash_t hash = RSHash (_pname);
    int i, j;
    DEBUGER ("%d %s", _inode, _pname);
    if (IDXBLK == NULL)
      {
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		return 0;	// no entry in the dir
	    }
	  LoadDirIndex (_inode);
      }
    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  if (IDXBLK[i].hash_ >= hash)
	    {
		if (IDXLEAF == NULL)
		  {
		      LoadLeafBlocks (IDXBLK + i);
		  }
		for (j = 0; j < LFS_BLOCK_NO; ++j)
		  {
		      if (IDXLEAF[j].hash_ == hash)
			{
			    inode_t inode =
				IsFindTarget (IDXLEAF[j].offset_, _pname);
			    if (inode > 0)
			      {
				  DEBUGER ("FindIt!%d", inode);
				  return IDXLEAF[j].offset_;
			      }
			}
		  }
		break;
	    }
      }
    return 0;
}

offset_t LoadSingleLeaf (offset_t _pos)
{
    dir_index_leaf_t *leaf = malloc (sizeof (dir_index_leaf_t));
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, leaf, sizeof (dir_index_leaf_t), _pos);
#else
    pread (lfs_n.fd, leaf, sizeof (dir_index_leaf_t), _pos);
#endif
    DEBUGER ("\t%lu %lu DISK", leaf->hash_, leaf->offset_);
    return leaf->offset_;
}

int Print_Index (inode_t _inode)
{
    int i = 0, j = 0;
    int cntIF = 0;
    int cntIB = 0;
    if (IDXBLK == NULL)
      {
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		DEBUGER ("NO INDEX");
		return 0;
	    }
	  LoadDirIndex (_inode);
      }
    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  offset_t off = GetIndexDiskPos (_inode, i, -1);
	  ALERTER ("\t%lu %lu [IB(%d)]", IDXBLK[i].hash_, IDXBLK[i].off_, i);
	  cntIB++;
	  LoadSingleLeaf (off);

	  if (IDXBLK[i].off_ == 0)
	      break;
	  if (IDXLEAF == NULL)
	      LoadLeafBlocks (IDXBLK + i);
	  for (j = 0; j < LFS_BLOCK_NO; ++j)
	    {
		if (IDXLEAF[j].offset_ == 0)
		    continue;
		off = GetIndexDiskPos (_inode, i, j);
		ALERTER ("\t%lu %lu [IF(%d)]", IDXLEAF[j].hash_,
			 IDXLEAF[j].offset_, j);
		cntIF++;
		off = LoadSingleLeaf (off);
		LoadDirEntry (off);
	    }
	  ALERTER ("STATISTIC %dIB %dIF", cntIB, cntIF);
      }
    return 0;
}

int Init_Dcache (inode_t _inode)
{
#ifdef DCACHEMODE
    int i = 0, j = 0;
    int cntIF = 0;
    int cntIB = 0;
    if (IDXBLK == NULL)
      {
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		DEBUGER ("NO INDEX");
		return 0;
	    }
	  LoadDirIndex (_inode);
      }
    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  offset_t off = GetIndexDiskPos (_inode, i, -1);
	  ALERTER ("\t%lu %lu [IB(%d)]", IDXBLK[i].hash_, IDXBLK[i].off_, i);
	  cntIB++;
	  LoadSingleLeaf (off);

	  if (IDXBLK[i].off_ == 0)
	      break;
	  if (IDXLEAF == NULL)
	      LoadLeafBlocks (IDXBLK + i);
	  for (j = 0; j < LFS_BLOCK_NO; ++j)
	    {
		if (IDXLEAF[j].offset_ == 0)
		    continue;
		off = GetIndexDiskPos (_inode, i, j);
		ALERTER ("\t%lu %lu [IF(%d)]", IDXLEAF[j].hash_,
			 IDXLEAF[j].offset_, j);
		cntIF++;
		off = LoadSingleLeaf (off);
		dir_entry_t *pdir = LoadDirEntry (off);
		int iRet = dcache_lookup (_inode, pdir->pname_);
		if (iRet != 0)
		  {
		  }
		else
		  {
		      dcache_insert (_inode, pdir->pname_, pdir->inode_);
		  }
	    }
	  ALERTER ("STATISTIC %dIB %dIF", cntIB, cntIF);
      }
#endif
    return 0;
}

int DeleteIndex (inode_t _inode, char *_pname)
{
    hash_t hash = RSHash (_pname);
    int i, j;
    DEBUGER ("finode%d %s", _inode, _pname);
    if (IDXBLK == NULL)
      {
	  if (DIRMETA_ARRAY[_inode].idx_blkptr_ == 0)
	    {
		return 0;	// no entry in the dir
	    }
	  LoadDirIndex (_inode);
      }
    for (i = 0; i < LFS_BLOCK_NO; ++i)
      {
	  if (IDXBLK[i].hash_ >= hash)
	    {
		if (IDXLEAF == NULL)
		  {
		      LoadLeafBlocks (IDXBLK + i);
		  }
		for (j = 0; j < LFS_BLOCK_NO; ++j)
		  {
		      if (IDXLEAF[j].hash_ == hash)
			{
			    inode_t inode =
				IsFindTarget (IDXLEAF[j].offset_, _pname);
			    if (inode > 0)
			      {
				  ALERTER ("FindIt!%d", inode);
				  ChangeLeafBLK (IDXLEAF + j,
						 GetIndexDiskPos (_inode, i,
								  j), 0, 0);
				  return inode;
			      }
			}
		  }
		break;
	    }
      }
    return 0;
}

#if 0
void test_index ()
{
    int i = 0;
//    char str[1000];
    for (i = 0; i < 1000; ++i)
      {
      }
}
#endif
