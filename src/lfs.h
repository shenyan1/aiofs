#ifndef _LFS_H
#define _LFS_H
#include "arc.h"
#include "lfs_define.h"
#define true 1
#define false 0
#include<unistd.h>
#include<inttypes.h>
#include<stdio.h>
#include"lfs_cache.h"
#include<stdlib.h>
#include<sys/time.h>
#include"config.h"
#include"lfs_waitqueue.h"
#include "lfs_freemap.h"
#include "lfs_dir.h"
#include "lfs_dirindex.h"
#include "lfs_dcache.h"
struct file_entry
{
    uint32_t fsize;
    uint8_t is_free;
    uint32_t ext_inode;
    uint64_t meta_table[57];
} __attribute__ ((__packed__));
typedef struct file_entry file_entry_t;
typedef struct read_entry
{
    uint64_t offset;
    int id;
    uint64_t size;
    int shmid;
} read_entry_t;

struct loginfo
{
    int fd;
    char *logfile;
};
typedef struct loginfo loginfo_t;
struct lfs_instance
{
    char fname[20];		// /dev/sdf11
    int fd;
};
typedef struct lfs_instance lfs_instance_t;
typedef struct lfs_info
{
    int fd;
    uint64_t off;
    file_entry_t *f_table;
    char *block_device;
#ifdef USE_SARC
    sarc_t *sarc_cache;
#else
    arc_t *arc_cache;
#endif
    dcache_hash_t dcache;
    int stopfs;
    pthread_cond_t stop_cv;
    pend_hash_t pending_queue;
    /*
     * by geners add freelist and add dir_meta_array
     */
    dir_inode_t *pdirinodelist;
    file_inode_t *pfileinodelist;
    free_node_t *pfreelist;
    dir_meta_t *pdirmarray;
    //free_node_t *pdirinode_list;


    char *pindexmap;

    dir_index_t **pindexlist;
    //char *freebitmap;
    //uint64_t *freemap;
    cache_t *lfs_cache;
    cache_t *lfs_obj_cache;
    cache_t *rq_cache;
    cache_t *dentry_cache;
/*  aio queue
 */
    CQ *aioq;
    CQ *req_queue;
    io_queue_info_t cq_info;
    uint32_t max_files;
/*  dispatcher_th: process requests from socket and dispatch to req_queue
 *  rfs_rworker_th: process requests in queue.
 */
    pthread_t rfs_dispatcher_th;
    pthread_t rfs_reqworker_th;
/*  dirworker: process dir operations.
 */
    pthread_t rfs_dirworker_th;
/* aio threads:
 * receiver thread: wait aio I/O return from disk.
 * worker thread: dispatch aio to disk.
 */
    pthread_t rfs_receiver_th;
    pthread_t rfs_worker_th;
    lfs_instance_t instance;
    loginfo_t log;
} lfs_info_t;
extern uint64_t getphymemsize (void);
void lfs_reopen ();
typedef struct kmutex
{
    void *m_owner;
    uint64_t m_magic;
    pthread_mutex_t m_lock;
} kmutex_t;

int response_client (int clifd, int value);
int response_client_str (int clifd, char *ptr, int len);

int rfs_return_data (char *proto, int len, int clifd);

uint64_t cur_usec (void);
extern uint64_t getlocalp (uint64_t id);
extern uint64_t arc_hash_init (void);
extern void lfs_mutex (pthread_mutex_t * lock);
int buf2id (char *ptr);
extern void lfs_unmutex (pthread_mutex_t * lock);
char *getshmptr (int shmid);
int lfs_genflock (char *filename);
void lfs_printf (const char *fmt, ...);
void lfs_printf_debug (const char *fmt, ...);
void lfs_printf_err (const char *fmt, ...);
int lfs_log_init ();
#endif
