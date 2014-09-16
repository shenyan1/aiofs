#include"eserver.h"
#include"lfs.h"
#include<assert.h>
#include<sys/shm.h>
#include"lfs_sys.h"
#include"lfs_fops.h"
#include "lfs_dir.h"
extern lfs_info_t lfs_n;
/* UNIX socket ==> request queue.
 */
int decode_readprotocol (char *pro, int clifd)
{
    CQ_ITEM *item = cqi_new ();
    char op;
    read_entry_t *ptr = (read_entry_t *) (pro + 1);
    op = *pro;
    assert (op == READ_COMMAND);
    item->fops = op;
    item->size = ptr->size;
    item->fid = ptr->id - 1 - LFS_FINODE_START;

    assert (item->fid >= 0);
    item->offset = ptr->offset;
    item->clifd = clifd;
    lfs_printf ("size =%" PRIu64 ",offset=%" PRIu64 ",id=%d,op=%d\n",
		item->size, item->offset, item->fid, op);
    cq_push (lfs_n.req_queue, item);
    return true;

}

/* decode protocol to request queue.
 * WRITE protocol: WRITE(1B)   |...| as same as read protocol
 */
int decode_writeprotocol (char *pro, int clifd)
{
    CQ_ITEM *item = cqi_new ();
    char op;

    read_entry_t *ptr = (read_entry_t *) (pro + 1);
    op = *pro;
    assert (op == WRITE_COMMAND);
    item->fops = op;
    item->size = ptr->size;
    item->fid = ptr->id - 1 - LFS_FINODE_START;
    assert (item->fid >= 0);
    item->offset = ptr->offset;
    item->clifd = clifd;
    item->shmid = ptr->shmid;
    assert (lfs_n.f_table[item->fid].is_free == LFS_NFREE);
    assert (lfs_n.f_table[item->fid].meta_table[0] != 0);
    lfs_printf ("size =%" PRIu64 ",offset=%" PRIu64
		",id=%d,op=%d,metatable=%" PRIu64 "\n", item->size,
		item->offset, item->fid, op,
		lfs_n.f_table[item->fid].meta_table[0]);
    cq_push (lfs_n.req_queue, item);
    return true;
}

int getfiles (char *buf)
{
    lfs_printf ("max_files =%d", lfs_n.max_files);
    return lfs_n.max_files;
}

void write_done (CQ_ITEM * item, int res)
{
    int ret, clifd = item->clifd;
//    printf("clifd = %d\n",clifd);
    if (clifd <= 0)
	res = -1;
    else
      {
	  ret = shmdt (item->_ptr);
	  printf ("ret =%d\n", ret);
	  if (ret == -1)
	    {
		lfs_printf_err
		    ("write call back failed to detach the share memory\n");
		assert (0);
	    }
	  lfs_printf ("write has been done\n");
	  response_client (clifd, res);
      }
}

static int stopfs (void)
{
    lfs_n.stopfs = true;
    cv_broadcast (&lfs_n.stop_cv);
    return true;
}

/* dispatcher thread
 */
/* inode_free:
 * true:  this inode is free.
 * false: this inode is non-free.
 */
int _inode_free (int id)
{
    int inode = id;
    if (inode < 0)
	return -LFS_INVALID;

    if (id < 0 || id > lfs_n.max_files)
	return false;
    if (lfs_n.f_table[id].is_free == LFS_FREE)
	return true;
    else
	return false;
}

int inode_free (char *buf, int clifd)
{
    int res, inode;
    char *ptr = buf;
    ptr = ptr + 1;
    inode = *(int *) ptr;
    lfs_printf ("id=%d ", inode);
    res = _inode_free (inode);
    response_client (clifd, res);
    return true;
}

/* 0: failed
 * 1: means success
 */
int RemoveFile (char *fname)
{
    int iRes;
    inode_t inode;
    inode = Open (fname);

    if (inode == 0)
	iRes = inode;
    else
      {
	  iRes = RemoveDir (fname);
	  if (iRes != 0)
	    {
		iRes = _removeFile (inode - 1);
	    }
      }
    return iRes;
}

/* the typical protocal is OP|len|string
 */
int process_dirrequest (char *buf, int clifd)
{

    int len, _len;
    char *fname, op, *outptr;
//    lfs_printf ("in the process_dir request\n");
    op = *buf;
    len = *(buf + 1);
    fname = buf + 2;
    inode_t inode = 0;
    int iRes = 0;
    lfs_printf ("fname=%s,op=", fname);
    switch (op)
      {
      case MKDIR_COMMAND:
	  if (strcmp (fname, "/") == 0)
	    {
		lfs_printf ("client try to make a /");
		iRes = 0;
	    }
	  else
	      iRes = MakeDir (fname);
	  lfs_printf ("make dir\n");
	  response_client (clifd, iRes);
	  break;
      case FOPEN_COMMAND:
	  lfs_printf ("open \n");
	  inode = Open (fname);
	  response_client (clifd, inode);
	  break;
      case RMDIR_COMMAND:
	  lfs_printf ("rm dir\n");
	  iRes = RemoveDir (fname);
	  response_client (clifd, iRes);
	  break;
      case LIST_COMMAND:
	  lfs_printf ("list dir\n");
	  outptr = PrintDir (fname);
	  lfs_printf("%s\n",outptr);
	  _len = strlen (outptr);
	  response_client_str (clifd, outptr, _len);
	  free (outptr);
	  break;
      case RMFILE_COMMAND:
	  lfs_printf ("remove a file\n");
	  iRes = RemoveFile (fname);
	  response_client (clifd, iRes);
	  break;
      case MKFILE_COMMAND:
	  lfs_printf ("make a file\n");
	  iRes = CreateFile (fname);
	  response_client (clifd, iRes);
	  break;
      case CLOSE_COMMAND:
	  lfs_printf ("close a file\n");
	  inode = *(int *) (buf + 1);
	  iRes = CloseFile (inode);
	  response_client(clifd,iRes);
      }
    return 0;
}

/* fallocate protocol: |FALLOCATE|size|inode
 */
int decode_fallocateprotocol (char *pro, int clifd)
{
    inode_t inode;
    int size, res;
    char op;

    op = *pro;
    pro += 1;
    assert (op == FALLOCATE_COMMAND);
    size = *(int *) pro;
    lfs_printf ("fallocate size=%d\n", size);
    pro += 4;
    inode = *(int *) pro;
    inode -= 1;
    lfs_printf ("inode=%d", inode);
    assert (inode >= 0);
    res = finode_fallocate (inode, size);
    return res;
}



int process_request (char *buf, int clifd)
{
    int ret, max_files = 0;
    char op;
    op = *buf;
    lfs_printf ("in the process_request,op= %d\n", op);
    switch (op)
      {
      case READ_COMMAND:
	  decode_readprotocol (buf, clifd);
	  break;
      case WRITE_COMMAND:
	  decode_writeprotocol (buf, clifd);
	  break;
      case GETFILES_COMMAND:
	  max_files = getfiles (buf);
	  response_client (clifd, max_files);
	  break;
      case STOP_FS:
	  lfs_printf ("going to stop fs\n");
	  stopfs ();
	  break;
      case ISFREE_COMMAND:
	  inode_free (buf, clifd);
	  break;
      case FALLOCATE_COMMAND:
	  lfs_printf ("going to fallocate protocol\n");
	  ret = decode_fallocateprotocol (buf, clifd);
	  response_client (clifd, ret);
	  break;
      };
    lfs_printf ("finished \n");
    return true;
}
