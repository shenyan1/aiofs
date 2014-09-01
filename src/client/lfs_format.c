/*
   To format the whole disk.
   The disk format is :Name（8B）	Version8B	FileNums（8B）
   */
#include<stdio.h>
#include<sys/stat.h>
#include<assert.h>
#include<stdlib.h>
#include<inttypes.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include "lfs_format_dir.h"
#include"../lfs.h"
#include "lfs_formatfile.h"

uint64_t tot_size = 0;
uint64_t
getFilesize (char *str)
{
    FILE *fp = fopen (str, "r");
    fseek (fp, 0L, SEEK_END);
    uint64_t size = ftell (fp);
    fclose (fp);
    return size;
}

uint64_t *freemap;
int fidx = 0;
/*
   The FS information have 8B
   */
int
FormatFSinformation (int fd)
{
    int n;
    n = sizeof (FSNAME) + sizeof (VERSION);
    lfs_printf ("n=%d", n);
    write (fd, FSNAME, sizeof (FSNAME));
    write (fd, VERSION, sizeof (VERSION));
    freemap = malloc (16 << 20);
    memset (freemap, 0, 16 << 20);
}

int
getmaxfiles ()
{
    int max_files;
    uint64_t size, res_sizes, offset;
    offset = LFS_DATA_DOMAIN;
    size = tot_size - offset;
    /*   reserve some free space for 1MB blocks.
     */
    res_sizes = size * 5 / 100;
    max_files = (size - res_sizes) / AVG_FSIZE;
    return max_files;
}


/*
   Format FileMetaData
   */
int
FormatFileEntry (int fd)
{
    int i = 0, max_files = 0, j = 0;
    char *buffer = malloc (FILE_ENTRYS * 8);
    memset (buffer, 0, FILE_ENTRYS * 8);
    uint64_t disk_off, tmpoff;
    uint64_t offset = LFS_DATA_DOMAIN;
    lfs_printf ("data domain%llu %llu\n", LFS_DIR_INDEX_ENTRY, offset);
    max_files = getmaxfiles ();
    disk_off = LFS_FILE_ENTRY;
    tmpoff = offset;
    lseek (fd, disk_off, SEEK_SET);
    lfs_printf ("disk_off =%d\n", disk_off);
    for (i = 0; i < MAX_FILE_NO; i++)
      {
	  if (i < max_files)
	    {
		if (i % 20 == 0)
		  {
		      int j = 0;
		      for (j = 0; j < 200; ++j)
			{
			    freemap[fidx++] = tmpoff + j * LFS_BLKSIZE;
//                                      pwrite(fd, buffer, sizeof(uint32_t), tmpoff+j*LFS_BLKSIZE);
			    /*reserved for freemap */
			}
		      tmpoff += AVG_FSIZE;
		      continue;
		  }

		FormatAttributeTime (fd, j);
		FormatBlkptr (fd, j, tmpoff);
		tmpoff = tmpoff + AVG_FSIZE;
		assert (tot_size >= tmpoff);
		j++;
		//      disk_off += FILE_ENTRYS * sizeof (uint64_t);
	    }
	  else
	    {

		FormatAttributeTime (fd, i);
		FormatBlkptr (fd, i, 0);
//                      pwrite (fd, buffer, (FILE_ENTRYS) * sizeof (uint64_t),
//                                      disk_off);
		//      disk_off += FILE_ENTRYS * sizeof (uint64_t);
	    }

      }
    free (buffer);
    return 0;
}

/*
 * This method is to build small spacemap. We put the reserved space into small spacemap.
 *  The large fs support entrys <=1M entrys. So setup the entries here.
 */
#define BLKPTRSIZE sizeof(uint64_t)

/*
void FormatDirEntry(int _fd)
{
	    int i=0;
		    char *buffer = malloc (DIR_META_SIZE);
			    memset (buffer, 0, DIR_META_SIZE);
				    uint64_t meta_off, data_off;
					    uint64_t offset = LFS_DIR_DOMAIN;
						    meta_off = LFS_DIR_ENTRY;
							    data_off = offset; 
								    
								    
								    pwrite(_fd, buffer, DIR_META_SIZE, meta_off);
									    meta_off+=DIR_META_SIZE;

										    
										    for (i = 1; i < MAX_DIR_NO; ++i)
												    {   
														        pwrite(_fd, buffer, DIR_META_SIZE, meta_off);
																        meta_off+=DIR_META_SIZE;
																		    }
											    free(buffer);
												    DEBUG_FUNC("format dir_meta succeeded");
													    lfs_printf(" Format Dir_Meta succeeded @%s: %d\n", __FILE__, __LINE__);
														}
														
														void InitRootDir(int _fd, uint64_t _off){
														    pwrite(_fd, &_off, sizeof(uint64_t), LFS_DIR_ENTRY + 4 + 4 + 8 + 3*8 );
														    }
*/
int
FormatSpaceEntry (int fd)
{
    uint64_t offset, size;
    uint64_t r_sizes;
    uint32_t max_files = getmaxfiles ();

    offset = LFS_DATA_DOMAIN;
    size = tot_size - offset;
    lfs_printf ("off=%" PRIu64 ",size=%" PRIu64 "\n", offset, size);

    if (size % AVG_FSIZE == 0)
	r_sizes = 0;
    else
	r_sizes = size - max_files * AVG_FSIZE;
    r_sizes /= (1024 * 1024);
    lfs_printf ("It can contains %d 200M files,and reserves %" PRIu64
		"M size \n", max_files, r_sizes);
    pwrite (fd, &max_files, sizeof (uint32_t), sizeof (uint64_t));
///
// format the freemap and freebitmap 
//

    char *freebitmap = malloc (FREEBITMAP_SIZE);
    memset (freebitmap, 0, FREEBITMAP_SIZE);
    pwrite (fd, freebitmap, FREEBITMAP_SIZE, LFS_FREEBITMAP_ENTRY);
///
// format the root_director 
//
    lfs_printf ("freemap %" PRIu64 "\n", freemap[0]);
    InitRootDir (fd, freemap[0]);
    freemap[0] = 0;		// set the freemap is not free     
    pwrite (fd, freemap, 16 << 20, LFS_SPACE_ENTRY);
    lfs_printf ("freemap %" PRIu64 "\n", freemap[0]);
//      for (i = 0; i < 1024; i++)
//              lfs_printf ("%d free is %" PRIu64 "\n", i, freemap[i]);
//      pwrite (fd, &ptr, sizeof (uint64_t), LFS_SPACE_ENTRY + fidx * BLKPTRSIZE);
    /// format the space_entry
    //by geners 2014-07-29 


    free (freemap);
}

int
main (int argc, char *argv[])
{
    int fd;
    int flag = O_WRONLY | O_CREAT;
    lfs_printf ("Going to Format LFS now\n");
    lfs_printf ("the totoal memsize is %" PRIu64 "\n", getphymemsize ());
//      lfs_printf ("test");
    tot_size = getFilesize (argv[1]);
    lfs_printf ("totsize=%" PRIu64 "\n", tot_size);
    if (argv[1] == NULL)
      {
	  lfs_printf ("The LFS request a block device\n ");
	  exit (1);
      }
    fd = open (argv[1], flag, 0666);
    FormatFSinformation (fd);

    /* by geners 2014-07-29 add dir */
    FormatDirEntry (fd);

    FormatFileEntry (fd);
    FormatSpaceEntry (fd);
    close (fd);
}
