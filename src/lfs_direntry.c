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
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"eserver.h"
#include "lfs_define.h"
#include "lfs_dir.h"
#include "lfs_dirindex.h"

extern lfs_info_t lfs_n;

#define DIRINODE_LIST lfs_n.pdirinodelist
#define DIRMETA_ARRAY lfs_n.pdirmarray
//#define ENTRY_DATA_SIZE (sizeof(uint32_t) + sizeof(char) +sizeof(uint32_t) + sizeof(uint16_t))
#define ENTRY_DATA_SIZE (sizeof(dir_entry_t)-sizeof(char *))
#define ENTRY_DATA_SIZE_TOT (sizeof(dir_entry_t))

void _LoadDirEntry (dir_entry_t ** ppentry, offset_t _off)
{
    if (_off < LFS_DATA_DOMAIN)
      {				//error off
	  DEBUGER ("Error off%lu", _off);
	  return;
      }
    dir_entry_t *pentry;
    *ppentry = malloc (sizeof (dir_entry_t));

    pentry = *ppentry;
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, pentry, ENTRY_DATA_SIZE, _off);
#else
    pread (lfs_n.fd, pentry, ENTRY_DATA_SIZE, _off);
#endif
    _off += ENTRY_DATA_SIZE;
    uint16_t len = pentry->len_ + 1;
    pentry->pname_ = malloc (len);
    memset (pentry->pname_, 0, len);
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, pentry->pname_, len - 1, _off);
#else
    pread (lfs_n.fd, pentry->pname_, len - 1, _off);
#endif
    DEBUGER (" id%lu off%lu %s from %llu", pentry->inode_, pentry->nextoff_,
	     pentry->pname_, _off);
#if 0
    lfs_printf ("####### id%lu off%lu pentry=%p,%s from %llu\n",
		pentry->inode_, pentry->nextoff_, pentry, pentry->pname_,
		_off);
#endif
}


dir_entry_t *LoadDirEntry (offset_t _off)
{
    if (_off < LFS_DATA_DOMAIN)
      {				//error off
	  printf ("Error off%lu\n", _off);
	  return NULL;
      }
    dir_entry_t *pentry = (dir_entry_t *) malloc (sizeof (dir_entry_t));
//    printf("pentry=%p before \n",pentry);
    if (pentry == NULL)
	return NULL;
#ifdef O_DIRECT_MODE

    _lfs_pread (lfs_n.fd, pentry, ENTRY_DATA_SIZE, _off);
#else
    pread (lfs_n.fd, pentry, ENTRY_DATA_SIZE, _off);
#endif
    _off += ENTRY_DATA_SIZE;
    int len = pentry->len_ + 1;
    pentry->pname_ = malloc (len);
    memset (pentry->pname_, 0, len);
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, pentry->pname_, len - 1, _off);
#else
    pread (lfs_n.fd, pentry->pname_, len - 1, _off);
#endif
    DEBUGER (" id%lu off%lu %s from %llu", pentry->inode_, pentry->nextoff_,
	     pentry->pname_, _off);

//    lfs_printf ("####### id%lu off%lu pentry=%p,%s from %llu\n", pentry->inode_, pentry->nextoff_,
//           pentry,pentry->pname_, _off);

    //  printf("pentry=%p after \n",pentry);
    return pentry;
}

int WriteDirEntry (dir_entry_t * _pe, offset_t _off)
{
    DEBUGER ("pos:%llu %s", _off, _pe->pname_);
    _lfs_pwrite (lfs_n.fd, _pe, ENTRY_DATA_SIZE, _off);
    _off += ENTRY_DATA_SIZE;
    uint16_t len = _pe->len_;
    _lfs_pwrite (lfs_n.fd, _pe->pname_, len, _off);
    return 1;
}

int WriteDirEntryNext (uint32_t _nextlen, offset_t _off)
{
    ALERTER ("No implement");
    return 0;
}

dir_entry_t *GetNextDirEntry (dir_entry_t * _pe)
{
    if (_pe == NULL || _pe->nextoff_ <= LFS_DATA_DOMAIN)
      {
	  DEBUGER ("ERROR off%lu", _pe->nextoff_);
	  return NULL;
      }
    dir_entry_t *pRet = LoadDirEntry ((offset_t) _pe->nextoff_);
    if (_pe->pname_ != NULL)
	free (_pe->pname_);
    free (_pe);
    return pRet;
}

dir_entry_t *InitDirEntry (inode_t _inode, char _filetype, uint32_t _nextoff,
			   const char *_pname)
{
    dir_entry_t *pe = malloc (sizeof (dir_entry_t));

    pe->inode_ = _inode;
    pe->filetype_ = _filetype;
    pe->nextoff_ = _nextoff;
    pe->len_ = strlen (_pname);
    pe->pname_ = strdup (_pname);

    return pe;
}
