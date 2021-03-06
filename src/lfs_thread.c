#include"lfs.h"
#include<pthread.h>
#include<stdlib.h>
#include<assert.h>
#include"lfs_thread.h"
#include"lfs_define.h"
#include"lfs_sys.h"
#define current_thread pthread_self
#define MUTEX_INIT 0
#define ITEMS_PER_ALLOC 64
extern lfs_info_t lfs_n;
void mutex_init (lmutex_t * mp)
{
    if (pthread_mutex_init (&mp->mutex, NULL) != 0)
      {
	  lfs_printf ("pthread mutex init failed\n");
      }
    mp->mp_owner = 0;
    mp->func = 0;
    mp->line = 0;
}

void mutex_destroy (lmutex_t * mp)
{
    int ret = pthread_mutex_destroy (&mp->mutex);
    if (ret != 0)
      {
	  perror ("problemis");
	  lfs_printf ("mutex destroy failed=%d", ret);
      }
    assert (ret == 0);
}

void mutex_enter (lmutex_t * mp, const char *str, int line)
{

#ifdef THREAD_DEBUG
    lfs_printf ("%s mutex enter %p", str, mp->mutex);
#endif
//    lfs_printf ("mutex lock=%p\n", mp->mp_owner);
//    lfs_printf ("current_thread=%p\n", current_thread ());
    assert (mp->mp_owner != current_thread ());
    pthread_mutex_lock (&mp->mutex);
    mp->mp_owner = current_thread ();
    mp->func = str;
    mp->line = line;
}

void mutex_exit (lmutex_t * mp, const char *str)
{

#ifdef THREAD_DEBUG
    lfs_printf ("%s mutex exit %p", str, mp);
#endif
    pthread_mutex_unlock (&mp->mutex);
    mp->mp_owner = MUTEX_INIT;
    // mp->line = 0;
    mp->func = 0;
}

int mutex_held (lmutex_t * mp)
{
    return mp->mp_owner == current_thread ();

}

inline void cv_wait (pthread_cond_t * cv, lmutex_t * lock)
{

    pthread_cond_wait (cv, &lock->mutex);

}

inline void cv_signal (pthread_cond_t * cv)
{
    pthread_cond_signal (cv);
}

inline void cv_broadcast (pthread_cond_t * cv)
{
    pthread_cond_broadcast (cv);
}

void cv_destroy (pthread_cond_t * cv)
{
    int ret;
    ret = pthread_cond_destroy (cv);
    if (ret != 0)
      {
	  perror ("cv_destroy:");
	  lfs_printf ("ret=%d", ret);
      }
    assert (ret == 0);
}

/*
 * Adds an item to a io queue.
 */
void cq_push (CQ * cq, CQ_ITEM * item)
{
    item->next = NULL;

    pthread_mutex_lock (&cq->lock);
    if (NULL == cq->tail)
	cq->head = item;
    else
	cq->tail->next = item;
    cq->tail = item;
//    lfs_printf ("push cq :cq=%p,cq->head=%p\n", cq, cq->head);
    pthread_cond_signal (&cq->cond);
    pthread_mutex_unlock (&cq->lock);
}

/*
 * Looks for an item on a connection queue, but doesn't block if there isn't
 * one.
 * Returns the item, or NULL if no item is available
 */
CQ_ITEM *cq_pop (CQ * cq)
{
    CQ_ITEM *item;

    pthread_mutex_lock (&cq->lock);
//    lfs_printf ("pop a elem,RFSRQ=%p,cq=%p\n", RFS_RQ, cq);
//    if ((item = cq->head) == NULL)
//      lfs_printf ("head is null\n");
    while ((item = cq->head) == NULL)
	pthread_cond_wait (&cq->cond, &cq->lock);
    if (NULL != item)
      {
	  cq->head = item->next;
	  if (NULL == cq->head)
	      cq->tail = NULL;
      }
    pthread_mutex_unlock (&cq->lock);

    return item;
}

void ioreq_init ()
{

    RFS_RQ = (CQ *) malloc (sizeof (CQ));
    pthread_mutex_init (&RFS_RQ->lock, NULL);
    pthread_cond_init (&RFS_RQ->cond, NULL);
    RFS_RQ->head = NULL;
    RFS_RQ->tail = NULL;
}


void aioq_init ()
{

    RFS_AIOQ = (CQ *) malloc (sizeof (CQ));
    pthread_mutex_init (&RFS_AIOQ->lock, NULL);
    pthread_mutex_init (&AIO_INFO.cqi_freelist_lock, NULL);
    AIO_INFO.cqi_freelist = NULL;
    RFS_AIOQ->aioreqs = 0;
    pthread_cond_init (&RFS_AIOQ->cond, NULL);
    RFS_AIOQ->head = NULL;
    RFS_AIOQ->tail = NULL;
}

void cqi_free (CQ_ITEM * item)
{
    pthread_mutex_lock (&lfs_n.cq_info.cqi_freelist_lock);
    item->next = lfs_n.cq_info.cqi_freelist;
    lfs_n.cq_info.cqi_freelist = item;
    pthread_mutex_unlock (&lfs_n.cq_info.cqi_freelist_lock);
}

/*
 * Returns a fresh connection queue item.
 */
CQ_ITEM *cqi_new (void)
{
    CQ_ITEM *item = NULL;
    pthread_mutex_lock (&lfs_n.cq_info.cqi_freelist_lock);
    if (lfs_n.cq_info.cqi_freelist)
      {
	  item = lfs_n.cq_info.cqi_freelist;
	  lfs_n.cq_info.cqi_freelist = item->next;
      }
    pthread_mutex_unlock (&lfs_n.cq_info.cqi_freelist_lock);

    if (NULL == item)
      {
	  int i;

	  /* Allocate a bunch of items at once to reduce fragmentation */
	  item = malloc (sizeof (CQ_ITEM) * ITEMS_PER_ALLOC);
	  if (NULL == item)
	      return NULL;

	  /*
	   * Link together all the new items except the first one
	   * (which we'll return to the caller) for placement on
	   * the freelist.
	   */
	  for (i = 2; i < ITEMS_PER_ALLOC; i++)
	      item[i - 1].next = &item[i];

	  pthread_mutex_lock (&lfs_n.cq_info.cqi_freelist_lock);
	  item[ITEMS_PER_ALLOC - 1].next = lfs_n.cq_info.cqi_freelist;
	  lfs_n.cq_info.cqi_freelist = &item[1];
	  pthread_mutex_unlock (&lfs_n.cq_info.cqi_freelist_lock);
      }

    return item;
}
