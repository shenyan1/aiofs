#include"../lfs.h"
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
#include"../lfs_test.h"
#include"../lfs_thread.h"
lfs_info_t lfs_n;


int
lfs_init ()
{
    int i = 0;
    lfs_n.fd = open (lfs_n.block_device, O_RDWR);
    lfs_n.off = 0;
    /* To create a arc cache with total memory / 2
     */
    lfs_n.f_table = malloc ((10 << 10) * sizeof (file_entry_t));
    for (i = 0; i < 10 << 10; i++)
        lfs_n.f_table[i].meta_table =
            malloc (FILE_ENTRYS * sizeof (uint64_t));

    LoadFileEntry ();
    return 0;
}
int
lfs_fini ()
{
    int i;
    close (lfs_n.fd);
    printf ("going to free the lfs\n");
    for (i = 0; i < MAX_FILE_NO; i++)
        free (lfs_n.f_table[i].meta_table);
    free (lfs_n.f_table);
    return 0;
}
void lfs_get_files(){
int i;
   for (i = 0; i < lfs_n.max_files; i++)
  	if (lfs_n.f_table[i].is_free == LFS_FREE){
        printf("contains %d files",i);
	return ;
   }

}
void
lfs_param (char *argv[])
{
    if (strcmp (argv[1], "files") == 0)
	lfs_get_files();
    else if (strcmp (argv[1], "w") == 0)
	;
    else
        printf ("invalid args:\n"
	"files:return total files\n");

}
int LoadFileEntry ()
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
          //printf ("%d file is free %d\n", i, isfree);
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
    lfs_param (argv);
    lfs_fini ();
    printf ("Congratulations,the lfs filesystem V2 run success\n");
    return 0;
}

