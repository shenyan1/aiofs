#ifndef __LFS_AIO_QUEUE
#define __LFS_AIO_QUEUE
struct trace_entry
{
    short devno;
    uint64_t startbyte;
    int bytecount;
    char rwType;
    CQ_ITEM *item;
};

extern void aio_init ();

#endif
