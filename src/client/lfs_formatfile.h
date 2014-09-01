#ifndef _LFS_FORMAT_FILE_H
#define _LFS_FORMAT_FILE_H
int FormatBlkptr (int fd, int inode, uint64_t off);
int FormatAttributeTime (int fd, int inode);
#endif
