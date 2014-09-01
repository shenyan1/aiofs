/*
 * THU hpclab jxy 2014-7-28
 */
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include "../lfs.h"
#include "../lfs_define.h"
#include "lfs_format_dir.h"

void
FormatDirEntry (int _fd)
{
    int i = 0;
    char *buffer = malloc (DIR_META_SIZE);
    memset (buffer, 0, DIR_META_SIZE);
    uint64_t meta_off;
    meta_off = LFS_DIR_ENTRY;

    /*
     * create root director
     */
    pwrite (_fd, buffer, DIR_META_SIZE, meta_off);
    meta_off += DIR_META_SIZE;


    for (i = 1; i < MAX_DIR_NO; ++i)
      {
	  pwrite (_fd, buffer, DIR_META_SIZE, meta_off);
	  meta_off += DIR_META_SIZE;
      }
    free (buffer);
    DEBUG_FUNC ("format dir_meta succeeded");
    //lfs_printf(" Format Dir_Meta succeeded @%s: %d\n", __FILE__, __LINE__);
}

void
InitRootDir (int _fd, uint64_t _off)
{
    pwrite (_fd, &_off, sizeof (uint64_t),
	    LFS_DIR_ENTRY + DIR_META_BLKPTR_OFF);
    uint32_t buff = 0;
    pwrite (_fd, &buff, sizeof (uint32_t), _off);
}


void
FormatDirIndex (int _fd)
{
    char *buffer = malloc (LFS_INDEXMAP_SIZE);
    memset (buffer, 0xff, LFS_INDEXMAP_SIZE);
    pwrite (_fd, buffer, LFS_INDEXMAP_SIZE, LFS_DIR_INDEX_BITMAP);
}
