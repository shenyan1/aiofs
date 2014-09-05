#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<inttypes.h>
#include<sys/types.h>
#include<sys/resource.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"lfs.h"
#include <signal.h>
#include"arc.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
#include"eserver.h"
#include "lfs_freemap.h"
#include "lfs_define.h"
#include "lfs_dir.h"
#include"lfs_sys.h"
#include"lfs_dirserver.h"
lfs_info_t lfs_n;



int check_right ()
{
    char fs_name[4], version[4];
    memset (fs_name, 0, 4);
    memset (version, 0, 4);
    pread (lfs_n.fd, fs_name, 4, lfs_n.off);
    lfs_n.off += 4;
    lfs_printf ("fs name=%s\n", fs_name);

    //      lfs_printf("version");
    pread (lfs_n.fd, version, 3, lfs_n.off);
    lfs_printf ("version is %s", version);
    return true;
}

/*  file inode range from 1+
 */
int file_load (int i)
{
    uint64_t ip;
    int size = sizeof (file_entry_t) - 57 * (sizeof (uint64_t));
    ip = getlocalp (i);
    /* skip time attribute.
     */
    ip += 3 * 8;
    pread (lfs_n.fd, &lfs_n.f_table[i], size, ip);
    ip += size;
    pread (lfs_n.fd, &lfs_n.f_table[i].meta_table, 57 * sizeof (uint64_t),
	   ip);
/*    lfs_printf ("ip=%d,file is free?%s,blkptr=%" PRIu64 "\n", ip,
              lfs_n.f_table[i].is_free == LFS_FREE ? "yes" : "no",
              lfs_n.f_table[i].meta_table[0]);
*/
    return true;
}

/* Load all files' metadata.
 */
int LoadFileEntry ()
{
    int i;
    uint32_t max_files;

/*    3*8(time attribute)| fsize|
     */

    pread (lfs_n.fd, &max_files, sizeof (uint32_t), sizeof (uint64_t));
    lfs_n.max_files = max_files;

    lfs_n.f_table = malloc ((lfs_n.max_files + 1) * sizeof (file_entry_t));
    lfs_printf ("support %d files\n", lfs_n.max_files);
    for (i = 0; i < lfs_n.max_files	//lfs_n.max_files;
	 ; i++)
      {
	  if (lfs_n.f_table[i].meta_table == NULL)
	    {
		lfs_printf ("error when malloc memory in %d", __LINE__);
		exit (1);
	    }
	  file_load (i);
      }
    return true;
}

void print_lfstable ()
{
    int i, idx;
    file_entry_t entry;
    lfs_printf
	("\n\t=================lfs's dnode table============================\n");
    //      for(i=0;i<MAX_FILE_NO;i++){
    for (i = 0; i < 53; i++)
      {
	  entry = lfs_n.f_table[i];
	  lfs_printf ("\n\tentry %d fsize=%d isfree=%d\n",
		      i, entry.fsize, entry.is_free);
	  lfs_printf ("\t\tmetadata\n");
	  for (idx = 0; idx < FILE_ENTRYS; idx++)
	    {
		lfs_printf ("%" PRIu64 " +",
			    lfs_n.f_table[i].meta_table[idx]);

	    }
      }

    lfs_printf
	("\n\t=================lfs's dnode table============================\n");
}

/* 
 *  Load Filesystem's freemap.
 */

int LoadSpaceEntry ()
{
    lfs_n.off = LFS_SPACE_ENTRY;
    /*8MB freemap can contain 2TB 1MB blocks */
    //lfs_n.freemap = malloc (16 << 20);


    //memset (lfs_n.freemap, 0, 16 << 20);
    //pread (lfs_n.fd, lfs_n.freemap, 16 << 20, lfs_n.off);
#if 0
    for (i = 0; i < 1024; i++)
      {
	  //        lfs_printf ("free space =%" PRIu64 "\n", lfs_n.freemap[i]);
      }
    //assert (0);
#endif
    return true;
}

int ioserver_init ()
{
    ioreq_init ();
    pend_hash_init ();


    if (pthread_create
	(&lfs_n.rfs_reqworker_th, NULL, lfs_rworker_thread_fn,
	 (void *) NULL) != 0)
      {
	  lfs_printf ("Create lfs dispatcher thread error!\n");
	  exit (1);
      }

    if (pthread_create
	(&lfs_n.rfs_dispatcher_th, NULL, lfs_dispatcher_thread_fn,
	 (void *) NULL) != 0)
      {
	  lfs_printf ("Create lfs dispatcher thread error!\n");
	  exit (1);
      }
    if (pthread_create
	(&lfs_n.rfs_dirworker_th, NULL, lfs_dirworker_thread_fn,
	 (void *) NULL) != 0)
      {
	  lfs_printf ("Create lfs dir worker thread error!\n");
	  exit (1);
      }
    return 0;
}

#ifndef _LFS_DAEMON
static void sigterm_handler (int f)
{
    printf ("find sigterm\n");
}
#else
static void sigterm_handler (int f)
{
    int ret;
    printf ("sigterm_ignored\n");
    lfs_fini ();
    exit (0);
}
#endif
static void sigpipe_handler (int f)
{
    lfs_printf ("broke pipe, from client\n");
}

void enable_corefile ()
{
    struct rlimit limit;
    limit.rlim_max = limit.rlim_cur = RLIM_INFINITY;
    if (setrlimit (RLIMIT_CORE, &limit) < 0)
      {
	  perror ("can't enable core file\n");
      }
}

int lfs_init (char *bdev)
{
    lfs_n.fd = open (lfs_n.block_device, O_RDWR);
    lfs_n.stopfs = false;
    lfs_n.off = 0;
    check_right ();		// 
    /* To create a arc cache with total memory / 2
     */
    uint64_t arc_size = getphymemsize () * 3 / 4;
    lfs_arc_init (arc_size);	//

    lfs_n.lfs_cache =
	cache_create ("arc_cache", 1 << 20, sizeof (char *), NULL, NULL,
		      arc_size);
    lfs_n.lfs_obj_cache =
	cache_create ("obj_cache", sizeof (struct object), sizeof (char *),
		      NULL, NULL, arc_size);
    lfs_n.dentry_cache =
	cache_create ("dentry_cache", sizeof (dir_entry_t), sizeof (char *),
		      NULL, NULL, arc_size);
    enable_corefile ();
    assert (pthread_cond_init (&lfs_n.stop_cv, NULL) == 0);
    LoadFileEntry ();
    /// geners 2014-07-30 
    // update freemap, add the freebitmap 
//      LoadSpaceEntry ();
    dcache_hash_init ();
    LoadDirMeta ();
    LoadFreemapEntry ();
    aio_init ();
    ioserver_init ();
    lfs_reopen ();
    signal (SIGPIPE, sigpipe_handler);
    signal (SIGTERM, sigterm_handler);

    return 0;
}

#if 0
int lfs_test_init ()
{
    ioserver_init ();
}
#endif
int lfs_fini ()
{
    close (lfs_n.fd);
    lfs_printf ("going to free the lfs\n");
#ifdef USE_SARC
    __sarc_destroy (lfs_n.arc_cache);
#else
    __arc_destroy (lfs_n.arc_cache);
#endif
    cache_destroy_shm (lfs_n.lfs_cache);
    cache_destroy (lfs_n.lfs_obj_cache);
    free (lfs_n.f_table);
    close (lfs_n.instance.fd);
    unlink (lfs_n.instance.fname);
    return 0;
}

int lfs_wait ()
{

    lmutex_t lock;
    mutex_init (&lock);
    mutex_enter (&lock, __func__, __LINE__);

    while (lfs_n.stopfs == false)
      {
	  cv_wait (&lfs_n.stop_cv, &lock);
      }

    mutex_exit (&lock, __func__);
    mutex_destroy (&lock);
    lfs_printf ("going to close raw filesystem\n");
    return true;
}

int main (int argc, char *argv[])
{
    //freopen("sys_log/log", "w", stdout);

    if (argv[1] != NULL)
	lfs_n.block_device = argv[1];
    else
      {
	  lfs_printf ("usage:r/w method [device name] [files] [threads]\n");
	  lfs_printf ("lfs require a block device\n");
	  exit (1);
      }
#ifdef _LFS_DAEMON
    daemonize (argv[1]);
#endif
    lfs_init (argv[1]);
    freemap_test ();
//    lfs_test_init ();
//    dir_test (argv[2]);
    lfs_wait ();
    lfs_fini ();
    return 0;
}
