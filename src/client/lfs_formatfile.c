#include<stdio.h>
#include<fcntl.h>
#include<stdlib.h>
#include<inttypes.h>
#include<sys/stat.h>
#include<assert.h>
#include<lfs_define.h>
#include"rfsio.h"
/* File Time Attribute: ctime,atime,mtime(3*8B),size,extflags
 * struct file_entry
{
    uint32_t fsize;
    uint8_t is_free;
    uint32_t ext_inode;
    uint64_t meta_table[57];
}
 */
uint64_t _getlocalp (uint64_t id)
{
    uint64_t lp;
    assert (id >= 0);
    lp = LFS_FILE_ENTRY;
    lp += id * METASIZE_PERFILE;
    return lp;
}

int
FormatAttributeTime (int fd, int inode)
{
    char buf[4096];
    memset (buf, 0, 4096);
    uint64_t pos = _getlocalp (inode);
    pwrite (fd, buf, 8 * 3 + 4 + 1 + 4, pos);
    return 1;
}

int
lfs_tell (int file)
{
    return lseek (file, 0, SEEK_CUR);
}

/* Format File blkptr.
 */
int
FormatBlkptr (int fd, int inode, uint64_t ptr)
{
    char buf[4096];
    memset (buf, 0, 4096);
    // format LargeBlock block.
    uint64_t pos = getlocalp (inode);
    pos += 3 * 8;
    pos += 4 + 1 + 4;
    printf ("format blkptr=%" PRIu64 ",off=%d\n", ptr, pos);
    pwrite (fd, &ptr, sizeof (uint64_t), pos);
    // format 56 small block.
    pwrite (fd, buf, 56 * sizeof (uint64_t), pos + 8);
    return 1;
}
