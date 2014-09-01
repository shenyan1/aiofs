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
#include"lfs_sys.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"eserver.h"
#include "lfs_define.h"
#include "lfs_dir.h"

extern lfs_info_t lfs_n;

void PushListNode (free_node_t * pnode, uint64_t _off, int _idx)
{
    free_node_t *pnew = malloc (sizeof (free_node_t));
    pnew->offset = _off;
    pnew->idx = _idx;
    pnew->next = NULL;
    if (pnode != NULL)
      {
	  pnode->next = pnew;
      }
    else
	DEBUGER ("Insert an value into an null pointer");
}


void InitListNode (free_node_t * _pnode, uint64_t _off, int _idx)
{
    _pnode->offset = _off;
    _pnode->idx = _idx;
    _pnode->next = NULL;
}

void InsertListNode (uint64_t _off, int _idx)
{
    free_node_t *pnew = malloc (sizeof (free_node_t));
    InitListNode (pnew, _off, _idx);
    pnew->next = lfs_n.pfreelist;
    lfs_n.pfreelist = pnew;
}

free_node_t *PopListNode (free_node_t * _phead)
{
    free_node_t *pres = _phead;
//      if(pres != NULL)
//              _phead = pres->next;
    return pres;
}


int LoadFreemapEntry ()
{
/// there are something unclear with freebitmap
//      _lfs_n.off = LFS_FREEBITMAP_ENTRY;

    //_lfs_n.freemap = malloc (FREEMAP_SIZE);       
    //memset (_lfs_n.freemap, 0, FREEMAP_SIZE);
//      pread (_lfs_n.fd, _lfs_n.freemap, FREEMAP_SIZE, _lfs_n.off);
    /// add freelist 

    lfs_n.off = LFS_FREEMAP_ENTRY;
    free_node_t *pnow = NULL;
    int i = 0;
    uint64_t disk_off = 0;
    lseek (lfs_n.fd, lfs_n.off, SEEK_SET);
    DEBUG_FUNC ("Freemap");
    for (i = 0; i < FREEBITMAP_SIZE; i++)
      {
	  read (lfs_n.fd, &disk_off, sizeof (uint64_t));
	  if (i < 10)
	      DEBUGER ("%llu", disk_off);
	  if (disk_off != 0)
	    {
		//lfs_n.off+=sizeof(uint64_t);
		//pread(lfs_n.fd, &disk_off, sizeof(uint64_t), lfs_n.off);
		if (pnow == NULL)
		  {
		      lfs_n.pfreelist = malloc (sizeof (free_node_t));
		      InitListNode (lfs_n.pfreelist, disk_off, i);
		      pnow = lfs_n.pfreelist;
		  }
		else
		  {
		      PushListNode (pnow, disk_off, i);
		      pnow = pnow->next;
		  }
	    }
      }
    freemap_test ();
    return 0;
}

offset_t GetfreePosByidx (int _idx)
{
    offset_t iRet = LFS_FREEMAP_ENTRY + _idx * sizeof (uint64_t);
    return iRet;
}

void SetFreeMapUsed (int _idx, offset_t _v)
{
    offset_t off = GetfreePosByidx (_idx);
    offset_t pos = _v;
    _lfs_pwrite (lfs_n.fd, &pos, sizeof (uint64_t), off);
}



int Free_Freemap (offset_t _off)
{
    DEBUGER ("this is not implement");
    return 0;
}

offset_t Malloc_Freemap ()
{
    offset_t iRet = 0;
    free_node_t *pfree = PopListNode (lfs_n.pfreelist);
    if (pfree == NULL)
      {
	  return iRet;
      }
    lfs_n.pfreelist = pfree->next;
    iRet = pfree->offset;
    SetFreeMapUsed (pfree->idx, 0);
    uint32_t buff = 0;
    DEBUGER ("%llu", iRet);
    _lfs_pwrite (lfs_n.fd, &buff, sizeof (uint32_t), iRet);
    free (pfree);
    return iRet;
}

void freemap_test ()
{
    free_node_t *pnode = lfs_n.pfreelist;
    DEBUGER ("BEGIN [freemap]");
    for (; pnode; pnode = pnode->next)
      {
	  if (pnode->idx < 10)
	      lfs_printf ("%d %llu\n", pnode->idx, pnode->offset);
      }
}
