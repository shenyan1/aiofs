#ifndef _LFS_TEST_H
#define _LFS_TEST_H



extern int test_write (int id, char *buffer, uint64_t size, uint64_t offset);

extern int test_read (int id, char *buffer, uint64_t size, uint64_t offset);
//extern int lfs_test_streamread (int files, int thdnums);
extern int lfs_test_write_all (char *buffer);

void lfs_test (char *argv[]);
//extern int lfs_test_read (int id);
extern int lfs_test_write (char *buffer);
#endif
