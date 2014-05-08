#define _XOPEN_SOURCE 500
#include<unistd.h>
#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<inttypes.h>
#include<sys/types.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"lfs.h"
#include"arc.h"
#include"lfs_test.h"
#include"lfs_thread.h"
#include"lfs_ops.h"
#include"lfs_fops.h"
#include"lfs_cache.h"
#include"aio_api.h"
lfs_info_t lfs_n;

char *testbuffer;

int
check_right ()
{
    char fs_name[4], version[4];
    memset (fs_name, 0, 4);
    memset (version, 0, 4);
    pread (lfs_n.fd, fs_name, 4, lfs_n.off);
    lfs_n.off += 4;
    printf ("fs name=%s\n", fs_name);

//      printf("version");
    pread (lfs_n.fd, version, 3, lfs_n.off);
    printf ("version is %s", version);
    return true;
}

int
LoadFileEntry1 ()
{
    printf ("load \n");
    return 1;
}

int
LoadFileEntry ()
{
    int i, md_idx;
    uint64_t fmetadata;
    char fname[8];
    uint32_t fsize, isfree, max_files;
/*    16 represent 8B:filename 4B:size 4B:iflag
 */
    pread (lfs_n.fd, &max_files, sizeof (uint32_t), sizeof (uint64_t));
    lfs_n.max_files = max_files;

    printf ("support %d files\n", lfs_n.max_files);
    lfs_n.off = LFS_FILE_ENTRY;
    for (i = 0; i < 10 << 10; i++)
      {
	  if (lfs_n.f_table[i].meta_table == NULL)
	    {
		printf ("error when malloc memory in %d", __LINE__);
		exit (1);
	    }
	  if (i == 0)
	      printf ("read meta %" PRIu64 "", lfs_n.off);
	  pread (lfs_n.fd, fname, sizeof (uint64_t), lfs_n.off);
	  lfs_n.off += sizeof (uint64_t);
	  pread (lfs_n.fd, &fsize, sizeof (uint32_t), lfs_n.off);
	  memcpy (lfs_n.f_table[i].filename, fname, 8);
	  lfs_n.f_table[i].fsize = fsize;
	  lfs_n.off += sizeof (uint32_t);
	  pread (lfs_n.fd, &isfree, sizeof (uint32_t), lfs_n.off);
	  printf ("%d file is free %d\n", i, isfree);
	  lfs_n.f_table[i].is_free = isfree;
	  lfs_n.off += sizeof (uint32_t);
	  for (md_idx = 0; md_idx < FILE_ENTRYS; md_idx++)
	    {
		pread (lfs_n.fd, &fmetadata, sizeof (uint64_t), lfs_n.off);
		lfs_n.f_table[i].meta_table[md_idx] = fmetadata;
		lfs_n.off += sizeof (uint64_t);
	    }

      }

    return true;
}

void
print_lfstable ()
{
    int i, idx;
    file_entry_t entry;
    printf
	("\n\t=================lfs's dnode table============================\n");
//      for(i=0;i<MAX_FILE_NO;i++){
    for (i = 0; i < 53; i++)
      {
	  entry = lfs_n.f_table[i];
	  printf ("\n\tentry %d filename=%s,fsize=%d isfree=%d\n",
		  i, entry.filename, entry.fsize, entry.is_free);
	  printf ("\t\tmetadata\n");
	  for (idx = 0; idx < FILE_ENTRYS; idx++)
	    {
		printf ("%" PRIu64 " +", lfs_n.f_table[i].meta_table[idx]);

	    }
      }

    printf
	("\n\t=================lfs's dnode table============================\n");
}

/* 
 *  Load Filesystem's freemap.
 */
int
LoadSpaceEntry ()
{
    lfs_n.off = LFS_SPACE_ENTRY;
    /*8MB freemap can contain 2TB 1MB blocks */
    lfs_n.freemap = malloc (16 << 20);

    memset (lfs_n.freemap, 0, 16 << 20);
    pread (lfs_n.fd, lfs_n.freemap, 16 << 20, lfs_n.off);
#if 0
    for (i = 0; i < 1024; i++)
      {
//	  printf ("free space =%" PRIu64 "\n", lfs_n.freemap[i]);
      }
    //assert (0);
#endif
    return true;
}

int
lfs_init ()
{
    int i = 0;
    lfs_n.fd = open (lfs_n.block_device, O_RDWR);
    lfs_n.off = 0;
    check_right ();
    testbuffer = malloc (1 << 20);
    /* To create a arc cache with total memory / 2
     */
    uint64_t arc_size = getphymemsize () / 4;
    lfs_arc_init (arc_size);
    lfs_n.f_table = malloc ((10 << 10) * sizeof (file_entry_t));
    for (i = 0; i < 1 << 20; i++)
      {
	  testbuffer[i] = i % 4 + '0';
      }
    for (i = 0; i < 10 << 10; i++)
	lfs_n.f_table[i].meta_table =
	    malloc (FILE_ENTRYS * sizeof (uint64_t));
    lfs_n.lfs_cache =
	cache_create ("arc_cache", 1 << 20, sizeof (char *), NULL, NULL,
		      arc_size);
    lfs_n.lfs_obj_cache =
	cache_create ("obj_cache", sizeof (struct object), sizeof (char *),
		      NULL, NULL, arc_size);
	
    LoadFileEntry ();
    print_lfstable ();
    LoadSpaceEntry ();
    aio_init();
    return 0;
}

int
lfs_fini ()
{
    int i;
    close (lfs_n.fd);
    printf ("going to free the lfs\n");
#ifdef USE_SARC
    __sarc_destroy (lfs_n.arc_cache);
#else
    __arc_destroy (lfs_n.arc_cache);
#endif
    cache_destroy (lfs_n.lfs_cache);
    cache_destroy (lfs_n.lfs_obj_cache);
    for (i = 0; i < MAX_FILE_NO; i++)
	free (lfs_n.f_table[i].meta_table);
    free (lfs_n.f_table);
    free (testbuffer);
    return 0;
}

void
lfs_test (char *argv[])
{
    if (strcmp (argv[1], "r") == 0)
	lfs_test_streamread ();
    else if (strcmp (argv[1], "w") == 0)
	lfs_test_write_all (testbuffer);
    else
	printf ("invalid args\n");

}

int
main (int argc, char *argv[])
{

    if (argv[1] != NULL)
	lfs_n.block_device = argv[2];
    else
      {
	  printf ("lfs require a block device\n");
	  exit (1);
      }
    lfs_init ();
    lfs_test (argv);
    lfs_fini ();
    printf ("Congratulations,the lfs filesystem V2 run success\n");
    return 0;
}
