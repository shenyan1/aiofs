#include"lfs.h"
#include<pthread.h>
#include<stdlib.h>
#include"lfs_thread.h"
void
mutex_init (pthread_mutex_t * mp)
{
    if (pthread_mutex_init (mp, NULL) != 0)
      {
	  printf ("pthread mutex init failed\n");
      }
}

void
mutex_destroy (pthread_mutex_t * mp)
{
    int ret = pthread_mutex_destroy (mp);
    assert (ret == 0);
}

void
mutex_enter (pthread_mutex_t * mp, const char *str)
{

#ifdef THREAD_DEBUG
    printf ("%s mutex enter %p", str, mp);
#endif
    pthread_mutex_lock (mp);
}

void
mutex_exit (pthread_mutex_t * mp, const char *str)
{

#ifdef THREAD_DEBUG
    printf ("%s mutex exit %p", str, mp);
#endif
    pthread_mutex_unlock (mp);
}

void
lfs_rw_enter (pthread_rwlock_t * lock, int rw)
{


}

inline void
cv_wait (pthread_cond_t * cv, pthread_mutex_t * lock)
{

    pthread_cond_wait (cv, lock);

}

inline void
cv_signal (pthread_cond_t * cv)
{
    pthread_cond_signal (cv);
}

inline void
cv_broadcast (pthread_cond_t * cv)
{
    pthread_cond_broadcast (cv);
}

void
cv_destroy (pthread_cond_t * cv)
{
    int ret;
    ret = pthread_cond_destroy (cv);
    assert (ret == 0);
}
