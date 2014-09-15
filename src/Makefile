CC    = gcc
PROGS = lfs_load
MAKE  = make iotool
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
#OBJS += client/rfsio.o
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread -laio
all: install lfs_load iotool lfs_sys.a
lfs_load:$(OBJS)
	cp lfs_define.h /usr/include/lfs_define.h
	$(CC) $^ -g -o $@ $(LIBS)
%.o:%.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm *.o
iotool:
	make -C client
	$(MAKE) -C client
lfs_sys.a:
	rm -f lfs_sys.a
	ar r lfs_sys.a lfs_sys.o
	ranlib lfs_sys.a
install:
	
	cp lfs_define.h /usr/include/lfs_define.h
