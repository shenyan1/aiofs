#include<stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <libaio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libaio.h>

int srcfd = -1;
int odsfd = -1;

#define AIO_BLKSIZE  1024
#define AIO_MAXIO 64

static void rd_done(io_context_t ctx, struct iocb *iocb, long res, long res2)
{
    
    int iosize = iocb->u.c.nbytes;
    char *buf = (char *)iocb->u.c.buf;
    off_t offset = iocb->u.c.offset;
    int tmp;
    char *wrbuff = NULL;

    if (res2 != 0) {
        printf("aio read\n");
    }
    if (res != iosize) {
        printf("read missing bytes expect % d got % d", iocb->u.c.nbytes, res);
        exit(1);
    }

    
    tmp = posix_memalign((void **)&wrbuff, getpagesize(), AIO_BLKSIZE);
    if (tmp < 0) {
        printf("posix_memalign222 \n");
        exit(1);
    }

    snprintf(wrbuff, iosize + 1, "%s", buf);

    printf("wrbuff_len = %d:%s \n", strlen(wrbuff), wrbuff);
    free(buf);

}

void main(int args, void *argv[])
{
    int length = sizeof("abcdefg");
    char *content = (char *)malloc(length);
    io_context_t myctx;
    int rc;
    char *buff = NULL;
    int offset = 0;
    int num, i, tmp;

    if (args < 3) {
        printf("the number of param is wrong \n");
        exit(1);
    }

    if ((srcfd = open(argv[1], O_RDWR)) < 0) {
        printf("open srcfile error \ n");
        exit(1);
    }

    printf("srcfd = %d \ n", srcfd);



    if ((odsfd = open(argv[2], O_RDWR)) < 0) {
        close(srcfd);
        printf("open odsfile error\n");
        exit(1);
    }

    memset(&myctx, 0, sizeof(myctx));
    io_queue_init(AIO_MAXIO, &myctx);

    struct iocb *io = (struct iocb *)malloc(sizeof(struct iocb));
    int iosize = AIO_BLKSIZE;
    tmp = posix_memalign((void **)&buff, getpagesize(), AIO_BLKSIZE);
    if (tmp < 0) {
        printf("posix_memalign error \n");
        exit(1);
    }
    if (NULL == io) {
        printf("io out of memeory \n");
        exit(1);
    }

    io_prep_pread(io, srcfd, buff, iosize, offset);

    io_set_callback(io, rd_done);

    printf("START...\n \n");

    rc = io_submit(myctx, 1, &io);

    if (rc < 0)
        printf("io_submit read error \n");

    printf("\nsubmit %d read request \n", rc);

    //m_io_queue_run(myctx);

    struct io_event events[AIO_MAXIO];
    io_callback_t cb;

    num = io_getevents(myctx, 1, AIO_MAXIO, events, NULL);
    printf("\n % d io_request completed \n \n", num);

    for (i = 0; i < num; i++) {
        cb = (io_callback_t) events[i].data;
        struct iocb *io = events[i].obj;
        printf("events[%d].data = %x, res = %d, res2 = %d \ n", i, cb, events[i].res, events[i].res2);
        cb(myctx, io, events[i].res, events[i].res2);
    }
}
