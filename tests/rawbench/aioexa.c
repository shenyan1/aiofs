#define _GNU_SOURCE  1
#include <unistd.h>  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <error.h>  
#include <errno.h>  
  
#include <fcntl.h>  
#include <libaio.h>  
  
int main(int argc, char *argv[])  
{  
    // 每次读入32K字节  
    const int buffer_size = 0x8000;  
  
    // 最大事件数 32  
    const int nr_events   = 32;  
    int rt,i;  
  
    io_context_t ctx = {0};  
  
    // 初使化 io_context_t  
    rt = io_setup(nr_events, &ctx);  
    if ( rt != 0 )  
        error(1, rt, "io_setup");  
  
    // 依次读取参数作为文件名加入提交到ctx  
    int pagesize = sysconf(_SC_PAGESIZE);  
    for ( i=1; i<argc; i++) {  
        struct iocb *cb = (struct iocb*)malloc(sizeof(struct iocb));  
        void *buffer;  
        // 要使用O_DIRECT, 必须要对齐  
        posix_memalign(&buffer, pagesize, buffer_size);  
        io_prep_pread(cb, open(argv[i], O_RDONLY | O_DIRECT), buffer, buffer_size, 0);  
        rt = io_submit(ctx, 1, &cb);  
        if (rt < 0)  
            error(1, -rt, "io_submit %s", argv[i]);;  
    }  
  
    struct io_event events[nr_events];  
    struct iocb     *cbs[nr_events];  
  
    int remain = argc - 1;  
    int n      = 0;  
  
    // 接收数据最小返回的请求数为1，最大为nr_events  
    while (remain && (n = io_getevents(ctx, 1, nr_events, events, 0))) {  
        int nr_cbs = 0;  
        for (i=0; i<n; ++i) {  
            struct io_event *event = &events[i];  
            struct iocb     *cb    = event->obj;  
            // event.res为unsigned  
            printf("%d receive %ld bytes\n", cb->aio_fildes, event->res);  
            if (event->	res > buffer_size) {  
                printf("%s\n", strerror(-event->res));  
            }  
            if (event->res != buffer_size || event->res2 != 0) {  
                --remain;  
                // 释放buffer, fd 与 cb  
                free(cb->u.c.buf);  
                close(cb->aio_fildes);  
                free(cb);  
            } else {  
                // 更新cb的offset  
                cb->u.c.offset += event->res;  
                cbs[nr_cbs++] = cb;  
            }  
        }  
  
        if (nr_cbs) {  
            // 继续接收数据  
            io_submit(ctx, nr_cbs, cbs);  
        }  
    }  
    return 0;  
}  

