#ifndef _LFS_TEST_H
#define _LFS_TEST_H



extern int test_create(uint64_t size);
    
extern int test_write(int id,char *buffer,uint64_t size,uint64_t offset);
    
extern int test_read(int id,char *buffer,uint64_t size,uint64_t offset);
extern int lfs_test_streamread(void);
extern int
lfs_test_write_all (char *buffer);

extern int lfs_test_read(int id);
extern int lfs_test_write(char *buffer);
#endif
