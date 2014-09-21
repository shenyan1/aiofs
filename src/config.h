#ifndef _CONFIG_H
#define _CONFIG_H
/*
  The config file
 */
//#define USE_SARC 0
#ifdef USE_SARC
#define PREFETCH_LENGTH 3
#define CONFIG_SHMEM 1
#endif

#define LFS_LOG_FNAME "/tmp/rfs_logs"

#define _LFS_DEBUG 1

#define _LFS_DAEMON 1

#endif
