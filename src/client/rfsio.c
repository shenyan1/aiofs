/* The method to read/write for user
 */
#include<assert.h>
#include<inttypes.h>
//#include"../eserver.h"
//#include"../lfs_dirserver.h"
#include<lfs_define.h>
#include<string.h>
#include<stdarg.h>
#include<unistd.h>
#include<net/if.h>
#include<stdio.h>
#include<sys/un.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/time.h>
#include<sys/shm.h>
#include<ifaddrs.h>
#include<sys/types.h>
#include<sys/socket.h>
#include"rfsio.h"
#define READ_SIZE (sizeof(char)+sizeof(int)+sizeof(int)+sizeof(uint64_t)+sizeof(uint64_t)+1)
#define _CLI_DEBUG
#define true 1
void _cli_printf (const char *fmt, ...)
{
#ifdef _CLI_DEBUG
    va_list ap;
    char *ret;
    int err;
    va_start (ap, fmt);
    err = vasprintf (&ret, fmt, ap);
    va_end (ap);
    printf (ret);
#endif
}
char *getshmptr (int shmid)
{
    char *ptr = 0;
    if (shmid > 0)
	ptr = shmat (shmid, NULL, 0);
    if (ptr == -1)
	perror ("what's wrong\n");
    return ptr;
}

uint64_t cur_usec (void)
{
    struct timeval _time;
    unsigned long long cur_usec;

    gettimeofday (&_time, NULL);
    cur_usec = _time.tv_sec;
    cur_usec *= 1000000;
    cur_usec += _time.tv_usec;

    return cur_usec;
}

/*READ protocol: READ(1B)     |Inode(4B)  |SHMID(4B)  |offset(8B)      |size(8B)*/
/*dir protocol:
 * OP|len|content
 */
/* return ptr: get buffer from clifd.
 */
char *
wait_io_str (int clifd)
{
    
    char *ptr;
    ptr = malloc(OUTPUT_MAXSIZE); 
    memset (ptr, 0, OUTPUT_MAXSIZE);
    read (clifd, ptr, OUTPUT_MAXSIZE);
    close (clifd);
    return ptr;
}

int
_rfs_send_dirrequest (char *proto, int len)
{
    int rcvbuf,optlen,rcv_size = OUTPUT_MAXSIZE,conn_sock = socket (AF_LOCAL, SOCK_STREAM, 0);
    if (conn_sock == -1)
      {
	  perror ("socket fail ");
	  exit (1);
	  return -1;
      }
    struct sockaddr_un addr;
    bzero (&addr, sizeof (addr));
    addr.sun_family = AF_LOCAL;
    strcpy ((void *) &addr.sun_path, DIR_UNIX_DOMAIN);
    optlen = sizeof(rcvbuf);
    if(setsockopt(conn_sock,SOL_SOCKET,SO_RCVBUF,(const char*)&rcv_size,sizeof(int))==-1)
	return -1;  
    if (connect (conn_sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
      {
	  perror ("connect fail ");
	  return -1;
      }
    if (write (conn_sock, proto, len) < 0)
      {
	  _cli_printf ("write error:send_request\n");
	  return (-1);

      }
    return conn_sock;
}


inline uint64_t
getdiskpos (uint64_t offset)
{
    uint64_t off;
    off = offset / LFS_BLKSIZE;
    off = off * LFS_BLKSIZE;

    assert (off <= offset);
    return off;
}

/* convert id list into protocol.
 */
char *
idlist2protocol (int *array, int len)
{
    char *ptr;
    int i;
    ptr = malloc (sizeof (char) + sizeof (int) * len - 1);
    *ptr = (char) array[0];
    for (i = 1; i < len; i++)
      {
	  *(int *) ptr = array[i];
      }
    return ptr;
}

char *
id2protocol (int shmid)
{
    char *ptr;
    ptr = malloc (sizeof (int));
    *(int *) ptr = shmid;
    return ptr;
}

int
wait_res (int connfd)
{
    int len, num;
    len = read (connfd, &num, sizeof (num));
    if (len <= 0)
      {
	  _cli_printf ("err read\n");
	  exit (1);
      }
    _cli_printf ("return res:shmid = %d\n", num);
    return num;
}
int
lsfs (char **argv)
{
    int ret, connfd, len;
    char *pro, *ptr,*_output,*fname;
    fname = *argv;
    if (strlen (fname) == 0 || fname == NULL)
	return -1;
    len = strlen (fname);

    len = 1 + 1 + len;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = LIST_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro = pro + 1;
    strcpy (pro, fname);

    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    _output = wait_io_str (connfd);
    if(_output!=NULL){
	printf("%s",_output);
        ret = LFS_OK;
    }
    else ret = -1;
    return ret;
   
}


int
_rfs_send_request (char *proto, int len)
{

    struct sockaddr_un addr;
    int conn_sock = socket (AF_LOCAL, SOCK_STREAM, 0);
    if (conn_sock == -1)
      {
	  perror ("socket fail ");
	  exit (1);
	  return -1;
      }
    bzero (&addr, sizeof (addr));
    addr.sun_family = AF_LOCAL;
    strcpy ((void *) &addr.sun_path, UNIX_DOMAIN);
    if (connect (conn_sock, (struct sockaddr *) &addr, sizeof (addr)) < 0)
      {
	  perror ("connect fail ");
	  return -1;
      }
    if (write (conn_sock, proto, len) < 0)
      {
	  _cli_printf ("write error:send_request\n");
	  return (-1);

      }
    return conn_sock;
}

/* rfs_close(): close a valid file.
 */
int
rfs_close (int fid)
{
    char *ptr, *pt;
    int res, connfd, maxfiles;

    ptr = malloc (sizeof (int) + sizeof (char));
    pt = ptr;
    *ptr = (char) CLOSE_COMMAND;
    ptr = ptr + 1;
    *(int *) ptr = fid;
    connfd = _rfs_send_dirrequest (pt, strlen (pt));
    res = wait_io (connfd);
    _cli_printf ("close fd\n");
    close (connfd);
    free (pt);
    _cli_printf ("close file finished\n");
    return res;
}

int
curmax_files (int fsid)
{
    char *ptr, *pt;
    int connfd, maxfiles;

    ptr = malloc (sizeof (int) + sizeof (char));
    pt = ptr;
    *ptr = (char) GETFILES_COMMAND;
    ptr = ptr + 1;
    *(int *) ptr = fsid;
    connfd = _rfs_send_request (pt, strlen (pt));
    maxfiles = wait_res (connfd);
    _cli_printf ("close fd\n");
    close (connfd);
    free (pt);
    _cli_printf ("finished curmax_files\n");
    return maxfiles;
}

/* return whether the file is free
*/
int
fdisfree (int fid)
{
    char *pt, *ptr;
    int connfd, isfree;
    if (fid <= 0)
      {
	  return LFS_INVALID;
      }
    pt = ptr = malloc (5 * sizeof (char));
    memset (ptr, 0, 5 * sizeof (char));
    *ptr = ISFREE_COMMAND;
    ptr = ptr + 1;
    *(int *) ptr = fid;
    connfd = _rfs_send_request (pt, strlen (pt));
    isfree = wait_res (connfd);
    _cli_printf ("id=%d\n", fid);
    close (connfd);
    free (pt);
    return isfree;
}



/* wait io return
*/
inline int
wait_io (int clifd)
{
    int ret;
    char *ptr, num[5] = { 0 };
    memset (num, 0, sizeof (num));
    read (clifd, num, sizeof (num));
    ptr = num;
    ret = *(int *) ptr;
//  _cli_printf ("ret value = %d\n", ret);
    close (clifd);
    return ret;
}

int
blockaligned (uint64_t p)
{
    return p % LFS_BLKSIZE == 0;
}

/*  request is in one block.
*/
char *
getbufptr (uint64_t offset, uint64_t size, char *buf)
{
    char *ptr;

    uint64_t off, loff, roff;
    ptr = buf;
    loff = getdiskpos (offset);
    roff = getdiskpos (offset + size);
    assert (roff == loff
	    || (blockaligned (offset + size)
		&& (roff - loff) <= LFS_BLKSIZE));
    off = offset - loff;
    ptr = buf + off;
    return ptr;
}

int
_tem (char *ptr)
{
    char *pro = ptr;
    int shmid;
    uint64_t offset, size;
    pro += 1;
    pro += sizeof (int);
    shmid = *(int *) pro;	//shmid is skipped.
    pro += sizeof (int);
    offset = *(uint64_t *) pro;
    pro += sizeof (uint64_t);
    size = *(uint64_t *) pro;
    return true;
}

void
lfs_memcpy (char *dstbuf, char *srcbuf, uint64_t off, uint64_t size)
{
    uint64_t end;
    int real_off;
    real_off = P2ALIGN (off, LFS_BLKSIZE);
    end = P2ALIGN (off + size, LFS_BLKSIZE);
    assert (P2ALIGN (off, LFS_BLKSIZE) == end
	    || ((end - real_off) == LFS_BLKSIZE
		&& ISALIGNED (off + size, LFS_BLKSIZE)));
    memcpy (dstbuf, srcbuf + off - real_off, size);

}

/* return: -1 means failed.
   	   0 means success
*/
int
_rfs_read (int id, char *buffer, uint64_t size, uint64_t offset)
{
    int shmid, connfd;
    read_entry_t *prt;
    char *pro, *buf, *ptr;
    pro = malloc (1 + sizeof (read_entry_t));
    memset (pro, 0, READ_SIZE);
    if (!buffer)
      {
	  _cli_printf ("error malloc");
	  exit (1);
      }
    ptr = pro;
    *pro = READ_COMMAND;
    pro = pro + 1;
    prt = (read_entry_t *) pro;
    prt->id = id;
    prt->offset = offset;
    prt->size = size;
    prt->shmid = 0;
/*    _cli_printf ("op=%d,id=%d off=%d,size=%d\n", READ_COMMAND, id, offset,
		size);
*/
    connfd = _rfs_send_request (ptr, sizeof (read_entry_t) + 1);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  exit (-1);
	  return -1;
      }
//  _cli_printf ("issue read\n");
    shmid = wait_io (connfd);
    buf = getshmptr (shmid);
    if (buf == NULL)
      {
	  _cli_printf ("shm buffer error when rfs_read\n");
	  return -1;
      }
//    buffer = getbufptr (offset, size, buf);
    lfs_memcpy (buffer, buf, offset, size);
    if (shmdt (buf) == -1)
      {
	  perror ("shmdt failed:");
	  return -1;
      }
//    _cli_printf ("read buf=%c%c%c", buffer[0], buffer[1], buffer[2]);
    return 0;
}


int rfs_read (int id, char *buffer, uint64_t size, uint64_t offset){

      uint64_t off,end;
      int i,nblks=0,tocpy,bufoff;

      end = offset + size;
      end = P2ROUNDUP(end,LFS_BLKSIZE);
      off = P2ALIGN(offset,LFS_BLKSIZE);
      nblks = (end - off) / LFS_BLKSIZE;

      if(nblks <= 0)
		return -1;
      else{
	for(i=0;i < nblks;i++){
		bufoff = offset - P2ALIGN(offset,LFS_BLKSIZE);
 		tocpy = MIN(LFS_BLKSIZE - bufoff,size);
		_rfs_read(id,buffer,tocpy,offset);
		offset += tocpy;
		buffer += tocpy;
		size   -= tocpy;
        }

	}
      return 0;
}
int
shm_malloc (int size)
{
    int shmid = shmget (IPC_PRIVATE, size, (SHM_R | SHM_W | IPC_CREAT));
    return shmid;
}

/* unfinished
 * rfs_write
 */
int
rfs_write (int id, char *buffer, uint64_t size, uint64_t offset)
{
    int res, shmid, connfd;
    read_entry_t *prt;
    char *pro, *buf, *ptr;
    pro = malloc (1 + sizeof (read_entry_t));
    memset (pro, 0, 1 + sizeof (read_entry_t));
    if (!buffer)
      {
	  printf ("error malloc");
	  return -1;
      }
    shmid = shmget (IPC_PRIVATE, size, (SHM_R | SHM_W | IPC_CREAT));
    if (shmid <= 0)
      {
	  printf ("allocate shm error!\n");
	  return -1;
      }
    buf = shmat (shmid, NULL, 0);

    memcpy (buf, buffer, size);
    ptr = pro;
    *pro = WRITE_COMMAND;
    pro = pro + 1;
    prt = (read_entry_t *) pro;
    prt->id = id;
    prt->offset = offset;
    prt->size = size;
    prt->shmid = shmid;

/*    printf ("write op=%d,id=%d off=%d,size=%d\n", WRITE_COMMAND, id,
		offset, size);
*/
    connfd = _rfs_send_request (ptr, sizeof (read_entry_t) + 1);
    if (connfd < 0)
      {
	  printf ("create conn socket failed");
	  return -1;
      }
    _cli_printf ("issue write\n");
    res = wait_io (connfd);
//    _cli_printf ("write buf=%c%c%c", buffer[0], buffer[1], buffer[2]);
    if (res == LFS_SUCCESS)
	_cli_printf ("write success");
    else
      {
	  _cli_printf ("rfs write failed\n");
	  return -1;
//	  assert (0);
      }
    if (shmctl (shmid, IPC_RMID, NULL) <0)
      {
	  perror ("shmctl RMID failed in rfs_write\n");
	  return -1;
      }
    return LFS_SUCCESS;
}

/* rfs_open(fname) : fname => inode
 * return 0: success
 * 	  -1: failed
 */
inode_t
rfs_open (char *fname)
{
    int connfd, len;
    int inode;
    char *pro, *ptr;
    if (strlen (fname) == 0 || fname == NULL)
	return LFS_FAILED;
    len = strlen (fname);
/*   |op type|size| fname|
 */
    len = len + 2;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = FOPEN_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro = pro + 1;
    strcpy (pro, fname);

    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    inode = wait_io (connfd);
//    _cli_printf ("op=open,fname=%s,ino=%d\n", fname, inode);
    return inode;
}

/* rfs_create protocol: |MKFILE_COMMAND|len|filename|.
 * return -1: fail
 *        0: success
 */
int
rfs_mkdir (char *fname)
{
    int ret, connfd, len;
    char *pro, *ptr;
    if (strlen (fname) == 0 || fname == NULL)
	return -1;
    len = strlen (fname);

    len = 1 + 1 + len;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = MKDIR_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro = pro + 1;
    strcpy (pro, fname);

    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    ret = wait_io (connfd);
    if (ret == 0)
	ret = -1;
    else{
	assert (ret == 1);
        ret = LFS_OK;
    }
    return ret;
}

/* rfs_create protocol: |MKFILE_COMMAND|len|filename|.
 * return -1: fail
 *        0: success
 */
int
rfs_create (char *fname)
{
    int connfd, len, res;
    char *pro, *ptr;
    if (strlen (fname) == 0 || fname == NULL)
      {
	  _cli_printf ("func:%s,invalied fname\n", __func__);
	  return LFS_FAILED;
      }
    len = strlen (fname);
    len = 1 + 1 + len;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = MKFILE_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro += 1;
    strncpy (pro, fname, strlen (fname));
    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    res = wait_io (connfd);
    if (res == 0)
      {
	  _cli_printf ("create file failed:%s\n", fname);
	  return LFS_FAILED;
      }
    else if (res >= 1)
      {
	  _cli_printf ("create file success:%s\n", fname);
      }
    return res;
}

/* fallocate protocol: |FALLOCATE|size|inode
 * fallocate support: if the file doesn't exist, create it.
 * preallocate a large file.
 * so fallocate use two socket to communicate with rfs daemon.
 * ret: -1 means this file doesn't existed.
 *       0 means fallocate success.
 * if not existed, create a file and fallocate.
 * else fallocate this file.
 */
int
rfs_fallocate (char *fname, int filesize)
{
    int res, connfd, len;
    char *pro, *ptr;
    inode_t inode;
    inode = rfs_open (fname);
    /*this file doesn't existed. */
    if (inode == 0)
      {
	  inode = rfs_create (fname);
	  /*if node == 0, create file failed. */
	  if (inode == LFS_FAILED)	/*failed again. */
	      return LFS_FAILED;
	  else
	      _cli_printf ("%s=>%d\n", fname, inode);
      }
    //   inode = rfs_open(fname);
    if (inode == 0)
      {
	  _cli_printf ("inode can't be 0\n");
	  exit (1);
	  return LFS_FAILED;
      }
    _cli_printf ("inode=%d in fallocate\n", inode);
    if (strlen (fname) == 0 || fname == NULL)
	return LFS_FAILED;
    len = sizeof (inode_t) + sizeof (int) + 1;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = FALLOCATE_COMMAND;
    pro = pro + 1;
    *(int *) pro = filesize;
    pro = pro + 4;
    *(int *) pro = inode;
    connfd = _rfs_send_request (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    res = wait_io (connfd);
//    _cli_printf ("op=fallocate,fname=%s\n", fname);
    if (res == LFS_FAILED)
      {
	  _cli_printf ("fallocate failed\n");
	  close (connfd);
	  return LFS_FAILED;
      }
    close (connfd);
    return LFS_OK;
}

/* rm a file protocol: |RMDIR_COMMAND|len|fname.
 * rm a regular file.
 * ret: -1 means this file doesn't existed.
 *       0 means remove success.
 */
int
rfs_remove (char *fname)
{
    int res, connfd, len;
    char *pro, *ptr;
    if (strlen (fname) == 0 || fname == NULL)
	return LFS_FAILED;
    len = strlen (fname);

    len = 1 + 1 + len;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = RMFILE_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro += 1;
    strncpy (pro, fname, strlen (fname));
    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    res = wait_io (connfd);

    _cli_printf ("rm a file.op=%d,fname=%s\n", RMFILE_COMMAND, fname);
    if (res == 0)
      {
	  close (connfd);
	  _cli_printf ("remove failed\n");
	  return LFS_FAILED;
      }
    else if (res == 1)
	_cli_printf ("remove success\n");
    close (connfd);
    return LFS_OK;
}

/* rmdir protocol: |RMDIR_COMMAND|len|fname.
 * fallocate support:
 * preallocate a large file.
 * ret: -1 means this file doesn't existed.
 *       0 means fallocate success.
 */
int
rfs_rmdir (char *fname)
{
    int res, connfd, len;
    char *pro, *ptr;
    if (strlen (fname) == 0 || fname == NULL)
	return LFS_FAILED;
    len = strlen (fname);

    len = 1 + 1 + len;
    pro = malloc (len);
    memset (pro, 0, len);
    ptr = pro;
    *pro = RMDIR_COMMAND;
    pro = pro + 1;
    *pro = strlen (fname);
    pro += 1;
    strncpy (pro, fname, strlen (fname));
    connfd = _rfs_send_dirrequest (ptr, len);
    if (connfd < 0)
      {
	  _cli_printf ("create conn socket failed");
	  return LFS_FAILED;
      }
    res = wait_io (connfd);

    _cli_printf ("rm a dir.op=%d,fname=%s\n", RMDIR_COMMAND, fname);
    if (res == 0)
      {
	  close (connfd);
	  _cli_printf ("rmdir failed\n");
	  return LFS_FAILED;
      }
    else if (res == 1)
	_cli_printf ("rmdir success\n");
    close (connfd);
    return LFS_OK;
}
int
stopfs ()
{
    int sock;
    char ch[10] = { 0 };
    ch[0] = STOP_FS;
    sock = _rfs_send_request (ch, 1);
    sleep (1);
    close (sock);
    return true;
}


