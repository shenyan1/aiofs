#ifndef __LFS_SYS_H
#define __LFS_SYS_H
#include<inttypes.h>
//#include"lfs_dir.h"
//#include"lfs.h"
uint64_t getlocalp (uint64_t id);
uint64_t cur_usec (void);
uint64_t getphymemsize ();
int buf2id (char *ptr);

void daemonize (char *cmd);

int lfs_trylock_fd (int fd);
int lfs_unlock_fd (int fd);
char *getshmptr (int shmid);

/* reserved for ls(large buffer output)
 */
#endif
