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
#define RW_DEFAULT	RW_READER
#include <libaio.h>
typedef pthread_t mown_t;
typedef struct lmutex{
	pthread_mutex_t mutex;
	mown_t mp_owner;
}lmutex_t;
typedef struct krwlock {
	void			*rw_owner;
	void			*rw_wr_owner;
	pthread_rwlock_t	rw_lock;
	int			rw_readers;
} krwlock_t;
typedef int krw_t;
void mutex_init(lmutex_t *mp);
void mutex_destroy(lmutex_t *mp);
extern void mutex_enter(lmutex_t *mp,const char *str);
extern void mutex_exit(lmutex_t *mp,const char *str);
extern void lfs_rw_enter(pthread_rwlock_t *lock,int rw);
inline void cv_signal(pthread_cond_t *cv);
inline void cv_broadcast(pthread_cond_t *cv);
inline void cv_wait(pthread_cond_t *cv,lmutex_t *mp);
inline void cv_destroy(pthread_cond_t *cv);
extern void rw_init(krwlock_t *rwlp, char *name, int type, void *arg);
extern void rw_destroy(krwlock_t *rwlp);
extern void rw_enter(krwlock_t *rwlp, krw_t rw);
extern int rw_tryenter(krwlock_t *rwlp, krw_t rw);
extern int rw_tryupgrade(krwlock_t *rwlp);
extern void rw_exit(krwlock_t *rwlp);

/*  structs for aio queue
 */
#define READ_COMMAND  0
#define WRITE_COMMAND 1

typedef struct conn_queue_item CQ_ITEM;
struct conn_queue_item {
    int size;
    int shmid;
    int fid;
    int fops;
    struct __arc_object *obj;
    uint64_t offset;
    CQ_ITEM *prev;
    CQ_ITEM *next;
};

typedef struct rfs_io_queue CQ;

struct rfs_io_queue {
    CQ_ITEM *head;
    CQ_ITEM *tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
};
struct io_queue
{
    struct iocb iocb;
    int ref_cnt;
    CQ_ITEM *item;
};

typedef struct io_queue IOCBQ;
typedef struct io_queue_info{
        pthread_mutex_t item_locks;
   	CQ_ITEM *cqi_freelist;
	pthread_mutex_t cqi_freelist_lock;
        IOCBQ *iocbq;
        pthread_mutex_t iocb_queue_mutex;
}io_queue_info_t;
extern CQ_ITEM *cq_pop(CQ *cq);

extern CQ_ITEM *cqi_new(void);
extern void cq_init(void);
extern void cq_push(CQ *cq, CQ_ITEM *item);
extern CQ_ITEM* cq_pop(CQ *cq);

#endif
