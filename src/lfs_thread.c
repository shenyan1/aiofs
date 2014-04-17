#include"lfs.h"
#include<pthread.h>
#include<stdlib.h>
#include"lfs_thread.h"
#define current_thread pthread_self
#define MUTEX_INIT 0
void mutex_init(lmutex_t *mp){
	if(pthread_mutex_init(&mp->mutex,NULL)!=0){
		printf("pthread mutex init failed\n");
	}
}
void mutex_destroy(lmutex_t *mp){
	int ret = pthread_mutex_destroy(&mp->mutex);
	assert(ret == 0);
}
void mutex_enter(lmutex_t *mp,const char *str){

#ifdef THREAD_DEBUG	
	printf("%s mutex enter %p",str,mp->mutex);
#endif
	assert(mp->mp_owner !=current_thread());
	pthread_mutex_lock(&mp->mutex);
	mp->mp_owner = current_thread();
}
void mutex_exit(lmutex_t *mp,const char *str){

#ifdef THREAD_DEBUG	
		printf("%s mutex exit %p",str,mp);
#endif
	pthread_mutex_unlock(&mp->mutex);
	mp->mp_owner = MUTEX_INIT;
}
int mutex_held(lmutex_t *mp){
	return mp->mp_owner == current_thread();

}

inline void cv_wait(pthread_cond_t *cv,lmutex_t *lock){
		
	pthread_cond_wait(cv,&lock->mutex);
	
}

inline void cv_signal(pthread_cond_t *cv){
	pthread_cond_signal(cv);
}

inline void cv_broadcast(pthread_cond_t *cv){
	pthread_cond_broadcast(cv);
}

void cv_destroy(pthread_cond_t *cv)
{
	int ret;
	ret=pthread_cond_destroy(cv);
	assert(ret==0);
}
