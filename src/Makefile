CC    = gcc
PROGS = lfs_load
MAKE  = make iotool
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
#OBJS += client/rfsio.o
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread -laio
all: lfs_load iotool
lfs_load:$(OBJS)
	$(CC) $^ -g -o $@ $(LIBS)
%.o:%.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm *.o
iotool:
	make -C client
	$(MAKE) -C client
