/*
   To format the whole disk.
   The disk format is :Name（8B）	Version8B	FileNums（8B）
 */
#include<stdio.h>
#include<assert.h>
#include<stdlib.h>
#include<inttypes.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<fcntl.h>
#include"../lfs.h"
uint64_t tot_size = 0;
uint64_t
getphymemsize ()
{
    return sysconf (_SC_PHYS_PAGES) * sysconf (_SC_PAGESIZE);
}

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
    printf ("n=%d", n);
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
  The FileEntry have (57+16)*10KB = 730KB
*/
int
FormatFileEntry (int fd)
{
    int i = 0, max_files = 0;
    char *buffer = malloc (FILE_ENTRYS * 8);
    memset (buffer, 0, FILE_ENTRYS * 8);
    uint64_t disk_off, tmpoff;
    uint64_t offset = LFS_DATA_DOMAIN;
    max_files = getmaxfiles ();
    disk_off = LFS_FILE_ENTRY;
    tmpoff = offset;

    for (i = 0; i < MAX_FILE_NO; i++)
      {

/*
        16 is the format of disk :8B for filename 4B:filesize 4B for padding
 */
	  pwrite (fd, buffer, 16, disk_off);
	  disk_off += 16;
	  if (i < max_files)
	    {
		if (i % 20 == 0)
		  {
		      freemap[fidx++] = tmpoff;
		      /*reserved for freemap */
		      tmpoff += AVG_FSIZE;
		  }


		printf ("file %d off=%" PRIu64 "\n", i, tmpoff);
		/*the LargeBlock blkptr */
		pwrite (fd, &tmpoff, sizeof (uint64_t), disk_off);
		tmpoff = tmpoff + AVG_FSIZE;

		assert (tot_size >= tmpoff);
		pwrite (fd, buffer, (FILE_ENTRYS - 1) * sizeof (uint64_t),
			disk_off + sizeof (uint64_t));
		disk_off += FILE_ENTRYS * sizeof (uint64_t);
	    }
	  else
	    {

		pwrite (fd, buffer, (FILE_ENTRYS) * sizeof (uint64_t),
			disk_off);
		disk_off += FILE_ENTRYS * sizeof (uint64_t);
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

int
FormatSpaceEntry (int fd)
{
    uint64_t i, offset, size;
    uint64_t r_sizes;
    uint64_t ptr = 0;
    uint32_t max_files;

    offset = LFS_DATA_DOMAIN;
    size = tot_size - offset;
    printf ("off=%" PRIu64 ",size=%" PRIu64 "\n", offset, size);

    if (size % AVG_FSIZE == 0)
	r_sizes = 0;
    else
	r_sizes = size - max_files * AVG_FSIZE;
    r_sizes /= (1024 * 1024);
    printf ("It can contains %d 200M files,and reserves %" PRIu64 "M size \n",
	    max_files, r_sizes);
    pwrite (fd, &max_files, sizeof (uint32_t), sizeof (uint64_t));

    pwrite (fd, freemap, 16 << 20, LFS_SPACE_ENTRY);
    for (i = 0; i < 1024; i++)
	printf ("%d free is %" PRIu64 "\n", i, freemap[i]);
    pwrite (fd, &ptr, sizeof (uint64_t), LFS_SPACE_ENTRY + fidx * BLKPTRSIZE);

    free (freemap);
}

main (int argc, char *argv[])
{
    int fd;
    int flag = O_WRONLY | O_CREAT;
    printf ("Going to Format LFS now\n");
    printf ("the totoal memsize is %" PRIu64 "\n", getphymemsize ());
    tot_size = getFilesize (argv[1]);
    printf ("totsize=%" PRIu64 "\n", tot_size);
    if (argv[1] == NULL)
      {
	  printf ("The LFS request a block device\n ");
	  exit (1);
      }
    fd = open (argv[1], flag, 0666);
    FormatFSinformation (fd);
    FormatFileEntry (fd);
    FormatSpaceEntry (fd);
    close (fd);
}
