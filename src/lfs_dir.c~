#define _GNU_SOURCE
#include"lfs_sys.h"
//#define _XOPEN_SOURCE 500
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
#include "lfs_direntry.h"
#include"eserver.h"
#include "lfs_define.h"
#include "lfs_dir.h"
#include "lfs_dirindex.h"

extern lfs_info_t lfs_n;

#define DIRINODE_LIST lfs_n.pdirinodelist
#define DIRMETA_ARRAY lfs_n.pdirmarray
#define DIRMETADATA lfs_n.pdirmarray[_inode]
#define P2ROUNDUP(x, align)	(-(-(x) & -(align)))
#define P2ALIGN(x, align)	((x) & -(align))
int IsDirorFile (inode_t inode)
{
    if (inode >= 0 && inode < LFS_FINODE_START)
      {
	  return 1;
      }
    if (inode >= LFS_FINODE_START)
      {
	  return 2;
      }
    return 0;
}

int lfs_malloc (void **_ptr, size_t _size)
{
    int res = posix_memalign (_ptr, 512, _size);
    if (res != 0)
      {
	  printf ("error\n");
      }
    return 0;
    //_ptr = malloc(_size); 
}

int _lfs_pread (int fd, void *_ptr, size_t _size, offset_t _pos)
{
    int eRet = 0;
#ifdef O_DIRECT_MODE
    offset_t begin = P2ALIGN (_pos, 512);
    offset_t end = P2ROUNDUP (_pos + _size, 512);
    int csize = end - begin;
    lfs_printf ("pos = %llo %llo %llo\n", _pos, begin, end);
    char *buf;
    lfs_malloc ((void **) &buf, csize);
    memset (buf, 0, csize);
    eRet = pread (fd, buf, end - begin, begin);
    memcpy (_ptr, (void *) buf + _pos - begin, _size);
    DEBUG_FUNC (buf);
    DEBUG_FUNC (_ptr);
    free (buf);
#else
    eRet = pread (fd, _ptr, _size, _pos);
#endif
    return eRet;
}

int _lfs_pwrite (int fd, void *_ptr, size_t _size, offset_t _pos)
{
    int eRet = 0;
#ifdef O_DIRECT_MODE
    offset_t begin = P2ALIGN (_pos, 512);
    offset_t end = P2ROUNDUP (_pos + _size, 512);
    int csize = end - begin;
    char *buf;
    lfs_printf ("pos = %o %o %o\n", _pos, begin, end);
    lfs_malloc ((void **) &buf, csize);
    eRet = pread (fd, buf, end - begin, begin);
    if (eRet == -1)
	return eRet;
    memcpy (buf + _pos - begin, _ptr, _size);
    eRet = pwrite (fd, buf, csize, begin);
    DEBUG_FUNC (buf);
    DEBUG_FUNC (_ptr);
    free (buf);
#else
    eRet = pwrite (fd, _ptr, _size, _pos);
#endif
    return eRet;
}


int LoadDirMeta ()
{
    int i = 0;
    lfs_n.off = LFS_DIR_ENTRY;
    DEBUG_FUNC ("Load");
    lfs_printf ("%lu\n", sizeof (dir_meta_t));

    lfs_n.pdirmarray = malloc (MAX_DIR_NO * sizeof (dir_meta_t));
    memset (lfs_n.pdirmarray, 0, sizeof (MAX_DIR_NO * sizeof (dir_meta_t)));
    lfs_n.off = LFS_DIR_ENTRY;
    lseek (lfs_n.fd, lfs_n.off, SEEK_SET);
    //load all meta
    free_node_t *pnow = NULL;
    for (i = 0; i < MAX_DIR_NO; ++i)
      {
	  read (lfs_n.fd, lfs_n.pdirmarray + i, DIR_META_SIZE);

	  if (IsIdleDirInode (lfs_n.pdirmarray[i].s_blkptr_))
	    {
		if (i < 10)
		    DEBUGER ("%d", i);
		if (DIRINODE_LIST == NULL)
		  {
		      DIRINODE_LIST = malloc (sizeof (free_node_t));
		      InitListNode (DIRINODE_LIST, 0, i);
		      pnow = DIRINODE_LIST;
		  }
		else
		  {
		      PushListNode (pnow, 0, i);
		      pnow = pnow->next;
		  }

	    }
      }
    //inodelist_test ();
    ///init dirindex
    InitDirTree ();
    return 1;
}

int WriteDirMeta (inode_t _inode)
{
#ifdef O_DIRECT_MODE
    _lfs_pwrite (lfs_n.fd, &DIRMETADATA, DIR_META_SIZE,
		 LFS_DIR_META_POS (_inode));
#else
    pwrite (lfs_n.fd, &DIRMETADATA, DIR_META_SIZE, LFS_DIR_META_POS (_inode));
#endif

    DEBUGER ("WRITEDIRMETA:%d, %llu %llu %llu", _inode,
	     LFS_DIR_META_POS (_inode), DIRMETADATA.s_blkptr_[0],
	     DIRMETADATA.s_blkptr_[1]);
    return 1;
}

int IsIdleDirInode (offset_t * _pblks)
{
    int i = 0;
    for (i = 0; i < 5; ++i)
      {
	  if (_pblks[i] != 0)
	      return 0;
      }
    return 1;
}

offset_t Inode2dir_meta (inode_t _inode)
{
    return 1;
}

offset_t GetDirEntryHead (offset_t _off)
{
    //offset_t iRet = 0;
    uint32_t off = 0;
    if (_off == 0)
      {				/// blkptr is empty
	  return 0;
      }
#ifdef O_DIRECT_MODE
    _lfs_pread (lfs_n.fd, &off, sizeof (uint32_t), _off);
#else
    pread (lfs_n.fd, &off, sizeof (uint32_t), _off);
#endif
    DEBUGER ("HEAD:%lu BLK:%llu", off, _off);
    if (off == 0)
      {
	  DEBUGER ("EMPTY BLK");
      }
    if (off < _off + 4 || off > _off + LFS_BLKSIZE)
      {				/// off is not initialize
	  DEBUGER ("ERROR Head");
	  return 0;
      }
    return (uint64_t) off;
}

void SetDirEntryHead (offset_t _blk, offset_t _data)
{
    uint32_t data = (uint32_t) _data;

    int t = 0;
#ifdef O_DIRECT_MODE
    t = _lfs_pwrite (lfs_n.fd, &data, sizeof (uint32_t), _blk);
#else
    t = pwrite (lfs_n.fd, &data, sizeof (uint32_t), _blk);
#endif
    DEBUGER ("write res%d HEAD %lu %llu", t, data, _blk);
}


inode_t FindSubdir (inode_t _pinode, const char *_pdir)
{
    int iRet;
    // first 
#ifdef DCACHEMODE

    iRet = dcache_lookup (_pinode, _pdir);
    if (iRet != 0)
      {
	  return iRet;
      }
    else
      {
	  iRet = FindIndex (_pinode, _pdir);
	  if (iRet != 0)
	      dcache_insert (_pinode, _pdir, iRet);
	  return iRet;
      }
#else
    iRet = FindIndex (_pinode, _pdir);
    return iRet;
#endif
    /*
       DEBUG_FUNC(_pdir);
       offset_t off;
       dir_entry_t pdir_e;
       for(i=0; i<5; ++i){//traverse all 5 block ;
       off = GetDirEntryHead(DIRMETA_ARRAY[_inode].s_blkptr_[i]);
       //pdir_e = LoadDirEntry(off);
       for (;false;){

       //if(strcmp (pdir_e->pname_, _pdir) == 0)
       //return pdir_e.inode_;
       //pdir_e = getnextdirentry(pdir_e);
       }
       return 1;
       }
       return 0;// no such dir named *_pdir
     */
}

int EnableSize (offset_t _head, offset_t _last, int _len, int _size)
{
    int iRet = (1 << 20) - (_last + 4 + 1 + 2 + 4 + _len - (_head + 4));
    if (iRet >= _size)
	return 1;
    return 0;
}

int AddDirEntry (inode_t _inode, const char *_pname)
{
    int nlen = strlen (_pname);
    offset_t blkoff = DIRMETA_ARRAY[_inode].s_blkptr_[0];
    offset_t off = GetDirEntryHead (blkoff), last = 0;

    dir_entry_t *pdir_e = NULL;
    DEBUG_FUNC (_pname);
    if (off != 0)
      {
	  last = off;
	  pdir_e = LoadDirEntry (off);
	  for (; true;)
	    {			// travel all space get the last value
		if (pdir_e == NULL)
		  {
		      ALERTER ("ERROR DirEntry");
		      return -1;
		  }
		if (pdir_e->nextoff_ == 0)
		    break;
		last = (offset_t) pdir_e->nextoff_;
		pdir_e = GetNextDirEntry (pdir_e);
	    }
	  DEBUGER ("_inode%d  last = %llu", _inode, last);
      }
    else
	last = blkoff;
    int size = 4 + 1 + 2 + 4 + nlen;
    if (off == 0 || EnableSize (off, last, pdir_e->len_, size))
      {
	  //DEBUG_FUNC(pdir_e->pname_);
	  //dir_inode_t * inode =  PopListNode(lfs_n.pdirinodelist);
	  offset_t pos_new = 0;	//_pdir_e->nextoff_;
	  //set last nextoff
	  if (off != 0)
	    {
		pos_new = last + 4 + 1 + 2 + 4 + pdir_e->len_;
		pdir_e->nextoff_ = (uint32_t) pos_new;
		WriteDirEntry (pdir_e, last);
	    }
	  else
	    {
		pos_new = blkoff + 4;
		SetDirEntryHead (blkoff, blkoff + 4);
	    }
	  // malloc a new idle dirinode
	  inode_t inode_new = MallocDirInode ();
	  /// initialize a direntry and write in disk
	  dir_entry_t *pdir_new = InitDirEntry (inode_new, 1, 0, _pname);
	  InsertIndex (_inode, _pname, pos_new);
	  WriteDirEntry (pdir_new, pos_new);
	  /// update the dir meta of new inode dir
	  DIRMETA_ARRAY[inode_new].s_blkptr_[0] = Malloc_Freemap ();
	  WriteDirMeta (inode_new);
	  //add index
	  DEBUGER ("finode %u ,inode %d, %s, %llu", _inode, inode_new, _pname,
		   pos_new);
	  if (pdir_new != NULL)
	      free (pdir_new);
	  return inode_new;
      }
    free (pdir_e);
    return 0;			// 0 is not 
}

offset_t Name2DirEntry (const char *_pdir)
{

    offset_t iRet = 0;
    if (_pdir[0] == 0)
	return iRet;
    // int len = strlen (_pdir);
    //if(_pdir[len-1] == '/')_pdir[len-1] = 0;
    int i = 0, j = 1;
    for (i = 1; _pdir[i]; ++i)
      {
	  if (_pdir[i] == '/')
	    {
		char *ptmp = strndup (_pdir + j, i - j);
		iRet = FindIndexOff (iRet, ptmp);
		free (ptmp);
		j = i + 1;
		if (iRet == 0)
		    return 0;
	    }
      }
    char *ptmp = strdup (_pdir + j);
    iRet = FindIndexOff (iRet, ptmp);
    free (ptmp);
    DEBUGER ("find entry off %llu ", iRet);
    return iRet;
}

inode_t Dir2Inode (const char *_pdir)
{
    /// pdir is  "/*/*/"
    DEBUG_FUNC (_pdir);
    inode_t iRet = 0;
    if (_pdir[0] == 0)
	return iRet;
    // int len = strlen (_pdir);
    //if(_pdir[len-1] == '/')_pdir[len-1] = 0;
    int i = 0, j = 1;
    for (i = 1; _pdir[i]; ++i)
      {
	  if (_pdir[i] == '/')
	    {
		char *ptmp = strndup (_pdir + j, i - j);
		iRet = FindSubdir (iRet, ptmp);
		free (ptmp);
		j = i + 1;
		if (iRet == 0)
		    return 0;
	    }
      }
    char *ptmp = strdup (_pdir + j);
    iRet = FindSubdir (iRet, ptmp);
    free (ptmp);
    return iRet;
}

char *FormatDirname (const char *_pname)
{
    if (_pname[0] != '/')
      {
	  SYS_OUT ("Is not an absolute_path");
	  return NULL;
      }
    int i = 0, len = strlen (_pname);

    for (i = 0; i < len - 1; ++i)
      {
	  if (_pname[i] == '/' && _pname[i + 1] == '/')
	    {
		SYS_OUT ("Valid dir or file name");
		return NULL;
	    }
      }

    char *pRet = NULL;
    if (_pname[len - 1] == '/')
	pRet = strndup (_pname, len - 1);
    else
	pRet = strdup (_pname);

    return pRet;
}

inode_t MakeDir (const char *_pname)
{
    int i = 0, j = 1;
    inode_t iRet = 0;
    inode_t f_inode = 0;	// initial the father inode is root_dir inode;
    //offset_t off = 0;

    //according to the pname get the father_dir_inode 
    char *pdir = FormatDirname (_pname);
    if (pdir == NULL)
	return 0;
    for (i = 1; pdir[i]; ++i)
      {
	  if (pdir[i] == '/')
	    {
		f_inode = FindSubdir (f_inode, strndup (pdir + j, i - j));
		j = i + 1;
		if (f_inode == 0)
		  {
		      SYS_OUT ("No Such Dir  %s", _pname);
		      return iRet;
		  }
	    }
      }
    /// add the dir entry in f_inode-dir
    char *pname = strdup (pdir + j);
    DEBUG_FUNC (pname);
    if (FindSubdir (f_inode, pname) != 0)
      {
	  SYS_OUT ("dir %s existed", pname);
	  return iRet;
      }

    for (i = 0; i < 5; ++i)
      {
	  iRet = AddDirEntry (f_inode, pname);
	  if (iRet > 0)
	    {
		SYS_OUT ("mkdir %s success, dir inode is %d", _pname, iRet);
		break;
	    }
	  if (iRet < 0)
	    {
		SYS_OUT ("mkdir failed");
		break;
	    }
      }
    free (pname);
    free (pdir);
    if (iRet < 0)
	iRet = 0;
    if (iRet > 0)
	iRet = 1;
    return iRet;
}

int AddDirEntryFile (inode_t _inode, const char *_pname)
{
    int nlen = strlen (_pname);
    offset_t blkoff = DIRMETA_ARRAY[_inode].s_blkptr_[0];
    offset_t off = GetDirEntryHead (blkoff), last = 0;

    dir_entry_t *pdir_e = NULL;
    DEBUG_FUNC (_pname);
    if (off != 0)
      {
	  last = off;
	  _LoadDirEntry (&pdir_e, off);
	  for (; true;)
	    {
		// travel all space get the last value
//              printf("in loop pdir_e=%p\n",pdir_e);
		if (pdir_e == NULL)
		  {
		      ALERTER ("ERROR DirEntry");
		      return -1;
		  }
		if (pdir_e->nextoff_ == 0)
		    break;
		last = (offset_t) pdir_e->nextoff_;
		pdir_e = GetNextDirEntry (pdir_e);
//              printf("GetNextDir pdir_e=%p\n",pdir_e);
	    }
	  DEBUGER ("_inode%d  last = %llu", _inode, last);
      }
    else
	last = blkoff;
    int size = 4 + 1 + 2 + 4 + nlen;
    if (off == 0 || EnableSize (off, last, pdir_e->len_, size))
      {
	  offset_t pos_new = 0;	//_pdir_e->nextoff_;
	  //set last nextoff
	  if (off != 0)
	    {

		pos_new = last + 4 + 1 + 2 + 4 + pdir_e->len_;
		pdir_e->nextoff_ = (uint32_t) pos_new;
		WriteDirEntry (pdir_e, last);
	    }
	  else
	    {
		pos_new = blkoff + 4;
		SetDirEntryHead (blkoff, blkoff + 4);
	    }
	  inode_t inode_new = finode_alloc ();
	  dir_entry_t *pdir_new = InitDirEntry (inode_new, 2, 0, _pname);

	  InsertIndex (_inode, _pname, pos_new);
	  WriteDirEntry (pdir_new, pos_new);

	  ///
	  // do filemeta
	  DEBUGER ("finode %u ,inode %d, %s, %llu", _inode, inode_new, _pname,
		   pos_new);
	  free (pdir_new);
	  return inode_new;
      }
    free (pdir_e);
    return 0;			// 0 is not 
}

int CreateFile (const char *_pname)
{
    int i = 0, j = 1;
    inode_t iRet = 0;
    inode_t f_inode = 0;	// initial the father inode is root_dir inode;
    //offset_t off = 0;

    //according to the pname get the father_dir_inode 
    char *pdir = FormatDirname (_pname);
    if (pdir == NULL)
	return 0;
    for (i = 1; pdir[i]; ++i)
      {
	  if (pdir[i] == '/')
	    {
		char *p = strndup (pdir + j, i - j);
		f_inode = FindSubdir (f_inode, p);
		j = i + 1;
		if (f_inode == 0)
		  {
		      SYS_OUT ("No Such File Path");
		      return iRet;
		  }
	    }
      }
    /// add the dir entry in f_inode-dir
    char *pname = strdup (pdir + j);
    DEBUG_FUNC (pname);
    if (FindSubdir (f_inode, pname) != 0)
      {
	  SYS_OUT ("file %s existed", pname);
	  return iRet;
      }

    for (i = 0; i < 5; ++i)
      {
	  iRet = AddDirEntryFile (f_inode, pname);
	  if (iRet > 0)
	    {
		SYS_OUT ("createfile success, file inode is %d", iRet);
		break;
	    }
	  if (iRet < 0)
	    {
		SYS_OUT ("createfile failed! maybe system is crushed");
		break;
	    }
      }
    free (pname);
    free (pdir);
    if (iRet < 0)
	iRet = 0;
//    if (iRet > 0)
//      iRet = 1;
    return iRet;
}

inode_t MallocDirInode ()
{
    int iRet = 0;
    free_node_t *pfree = PopListNode (DIRINODE_LIST);
    if (pfree == NULL)
      {
	  ALERTER ("No");
	  return iRet;
      }
    DIRINODE_LIST = pfree->next;
    iRet = pfree->idx;
    DEBUGER ("!!!%d %u", iRet, iRet);
    if (iRet == 0)
      {
	  DEBUGER ("ERROR");
      }
    free (pfree);
    return (inode_t) iRet;
}

int PrintDir (const char *_pdir)
{
    int len = strlen (_pdir);
    int i = 0;
    int cnt = 0;
    inode_t inode = 0;
    if (_pdir[0] != '/')
      {
	  ALERTER ("%s is invalid dir name", _pdir);
      }
    char *ptmp = FormatDirname (_pdir);
    if (ptmp == NULL)
	return 0;

    if (_pdir[len - 1] != '/')
      {
	  ptmp = strndup (_pdir, len);
	  inode = Dir2Inode (ptmp);
      }
    else
      {
	  ptmp = strndup (_pdir, len - 1);
	  inode = Dir2Inode (ptmp);
      }
    free (ptmp);

    if (IsDirorFile (inode) != 1)
      {
	  SYS_OUT ("No such dir");
	  return 0;
      }
    if (_pdir[0] != '/' || len != 1)
      {
	  if (inode == 0)
	    {
		SYS_OUT ("No such dir");
		return 0;
	    }
      }

    //lfs_printf ("[PRINTDIR name] = %s inode:%d", _pdir, inode);
    dir_entry_t *pdir_e = NULL;
    for (i = 0; i < 5; ++i)
      {
	  DEBUGER ("%d, %llu ", inode, lfs_n.pdirmarray[inode].s_blkptr_[i]);
	  offset_t off =
	      GetDirEntryHead (lfs_n.pdirmarray[inode].s_blkptr_[i]);
	  if (off == 0)
	      continue;
	  pdir_e = LoadDirEntry (off);
	  SYS_OUT ("%s:[inode]%d type %s", pdir_e->pname_, pdir_e->inode_,
		   pdir_e->filetype_ == 1 ? "dir" : "file");
	  cnt++;
	  for (; pdir_e != NULL && pdir_e->nextoff_ != 0;)
	    {
		pdir_e = GetNextDirEntry (pdir_e);
		SYS_OUT ("%s:[inode]%d type %s", pdir_e->pname_,
			 pdir_e->inode_,
			 pdir_e->filetype_ == 1 ? "dir" : "file");
		cnt++;
	    }
      }
    SYS_OUT ("%d entries have been list", cnt);
    if (pdir_e != NULL)
	free (pdir_e);
    return 1;
}

int IsEmptyDir (inode_t _inode)
{
    int i = 0;
    for (i = 0; i < 5; ++i)
      {
	  if (DIRMETADATA.s_blkptr_[i] != 0
	      && GetDirEntryHead (DIRMETADATA.s_blkptr_[i]) != 0)
	    {
		return 0;
	    }
      }
    return 1;
}

inode_t GetFatherInode (const char *_pdir)
{
    inode_t iRet = 0;
    char *pdir = strdup (_pdir);
    int len = strlen (pdir);
    if (pdir[len - 1] == '/')
	pdir[len - 1] = 0;
    char *p = rindex (pdir, '/');

    p = strndup (pdir, p - pdir);
    iRet = Dir2Inode (p);
    DEBUGER ("%s %d", pdir, iRet);
    free (pdir);
    free (p);
    return iRet;
}

/* return 0 means failed
 * return 1 means success
 */
int RemoveDir (const char *_pdir)
{				// remove a empty dir
    inode_t inode, finode = 0;
    char *pdir = FormatDirname (_pdir);
    if (pdir == NULL)
      {
	  return 0;
      }
    //int len = strlen (pdir);
    inode = Dir2Inode (pdir);
    int dir_type = IsDirorFile (inode);
    if (dir_type == 1)
      {
	  if (!IsEmptyDir (inode))
	    {
		SYS_OUT ("failed: %s isn't empty", pdir);
		return 0;
	    }
	  DEBUGER ("%s is empty dir", pdir);
      }

    if (dir_type == 2)
      {
	  if (!check_file_valid (inode))
	    {
		SYS_OUT ("failed: %s can't be delete", pdir);
		return 0;
	    }
	  DEBUGER ("%s is a deletable file", pdir);
      }

    if (dir_type == 0)
      {
	  SYS_OUT ("failed: %s is not found", pdir);
	  return 0;
      }

    finode = GetFatherInode (pdir);

    char *pname = rindex (pdir, '/') + 1;

    offset_t blkoff = DIRMETA_ARRAY[finode].s_blkptr_[0];
    offset_t off = GetDirEntryHead (blkoff), last = 0, now = 0;
    dir_entry_t *pe = NULL;
    if (off != 0)
      {
	  now = blkoff;
	  pe = LoadDirEntry (off);
	  DEBUGER ("%llu, %s %s", last, pe->pname_, pname);
	  if (strcmp (pe->pname_, pname) == 0)
	    {
#ifdef DCACHEMODE
		ALERTER ("remove dcache");
		dcache_remove (finode, pname);
#endif
		DeleteIndex (finode, pname);
		SetDirEntryHead (blkoff, pe->nextoff_);
		return 1;
	    }
	  else
	    {
		now += 4;
		for (;;)
		  {
		      if (strcmp (pe->pname_, pname) == 0)
			{
#ifdef DCACHEMODE
			    ALERTER ("remove dcache");
			    dcache_remove (finode, pname);
#endif
			    DeleteIndex (finode, pname);
			    SetDirEntryHead (last + 4 + 1, pe->nextoff_);
			    return 1;
			}
		      if (pe->nextoff_ == 0)
			{
			    break;
			}
		      last = now;
		      now = pe->nextoff_;
		      DEBUGER ("%s %s  %llu %llu", pe->pname_, pname, last,
			       now);
		      pe = GetNextDirEntry (pe);
		  }
	    }
      }
    free (pdir);
    return 0;
}

inode_t Open (const char *_pname)
{
    char *pdir = FormatDirname (_pname);
    if (pdir == NULL)
      {
	  return 0;
      }
    inode_t iRet = Dir2Inode (pdir);
    if (iRet != 0)
      {
	  if (iRet > LFS_FINODE_START)
	      SYS_OUT ("Open %s Success %d", _pname, iRet);
	  else
	      SYS_OUT ("%s is a dirname", _pname);
      }
    else
	SYS_OUT ("Open %s failed: no such file", _pname);
    return iRet;
}

void dir_test (char *_pname)
{
    int i = 0;
    char str[1000];
    int iRes = MakeDir ("/aa");
    if (iRes)
      {
	  for (i = 0; i < 5; ++i)
	    {
		sprintf (str, "/aa/t%d/", i);
		MakeDir (str);
	    }
      }
#if 0
    char buf[10];
    for (i = 0; i < 1000; i++)
      {
	  memset (buf, 0, 10);
	  sprintf (buf, "/f%d", i);
	  int iRes = CreateFile (buf);
	  if (iRes == 0)
	    {
		lfs_printf ("not success\n");
//        return ;
	    }
      }
#endif
    MakeDir ("/aa/");

    while (true)
      {
	  lfs_printf
	      ("[NOTE]###########\n p is print\n m is make\n o is open a file\n r is remove\n c is createfile\n");
	  char op[2], pname[100];
	  int arg = 0;
	  scanf ("%s", op);

	  if (op[0] == 'm')
	    {
		scanf ("%s", pname);
		DEBUGER ("-----------MAKE_DIR-----------");
		MakeDir (pname);
	    }
	  if (op[0] == 'p')
	    {
		scanf ("%s", pname);
		DEBUGER ("-----------PRINT_DIR-----------");
		PrintDir (pname);
	    }
	  if (op[0] == 'i')
	    {
		scanf ("%d", &arg);
		DEBUGER ("-----------PRINT_INDEX-----------");
		Print_Index (arg);
	    }
	  if (op[0] == 'r')
	    {
		scanf ("%s", pname);
		DEBUGER ("-----------REMOVE_DIR-----------");
		RemoveDir (pname);
	    }
	  if (op[0] == 'o')
	    {
		scanf ("%s", pname);
		DEBUGER ("-----------OPEN FILE-----------");
		Open (pname);
	    }
	  if (op[0] == 'c')
	    {
		scanf ("%s", pname);
		DEBUGER ("-----------CREATE FILE-----------");
		CreateFile (pname);
	    }
      }
    //Open("/aaa/aa/a");
}

void inodelist_test ()
{
    dir_inode_t *pnode = DIRINODE_LIST;
    for (; pnode; pnode = pnode->next)
      {
	  if (pnode->idx < 10)
	      lfs_printf ("free_dir_ inode%d\n", pnode->idx);
      }
}
