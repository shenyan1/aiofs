#ifndef _LFS_OPS_H
#define _LFS_OPS_H
#include"config.h"
#include"sarc.h"
#include"arc.h"
/*
 The file is implement the lfs's user operations
 It contains :
   int file_create(size);
   int file_write(int id,char *buffer,size);
   int file_open(int id,int flag);
   int file_remove(int id);
 */
#define READ_STATE    1
#define READ_HALF_FINISHED 2
#define READ_FINISHED 3
#define READ_GHOST    10

typedef struct object_data obj_data_t;
struct object_data
{
    char *data;
    int shmid;
};
struct object
{
    uint64_t id;
    uint64_t offset;
#ifdef USE_SARC
    struct __sarc_object entry;
#else
    struct __arc_object entry;
#endif
    obj_data_t *obj_data;
    int state;
};

extern struct object *getobj (struct __arc_object *e);
extern int file_create (int size);
extern int file_write (int id, char *buffer, uint64_t size, uint64_t offset);
extern int file_read (int id, char *buffer, uint32_t size, uint64_t offset);
extern int file_open (int id, int flag);
extern int file_remove (int id);
extern void lfs_arc_init (uint64_t arc_size);
struct object *getobj (struct __arc_object *e);
extern struct __arc_ops arc_ops;
#endif
