#ifndef _LFS_THREAD_H
#define _LFS_THREAD_H
#define	MTX_MAGIC	0x9522f51362a6e326ull
#define	MTX_INIT	((void *)NULL)
#define	MTX_DEST	((void *)-1UL)
#define	MUTEX_DEFAULT	0
#include<pthread.h>
#define RW_READER 0
#define RW_WRITER 1
#define OBJ_LOCK   1
#define OBJ_UNLOCK 0
#define OBJ_INIT   2
void mutex_init(pthread_mutex_t *mp);
void mutex_destroy(pthread_mutex_t *mp);
extern void mutex_enter(pthread_mutex_t *mp,const char *str);
extern void mutex_exit(pthread_mutex_t *mp,const char *str);
extern void lfs_rw_enter(pthread_rwlock_t *lock,int rw);
inline void cv_signal(pthread_cond_t *cv);
inline void cv_broadcast(pthread_cond_t *cv);
inline void cv_wait(pthread_cond_t *cv,pthread_mutex_t *mp);
inline void cv_destroy(pthread_cond_t *cv);
#endif
