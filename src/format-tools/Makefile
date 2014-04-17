CC    = gcc
PROGS = lfs_load
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread
all: lfs_format
lfs_load:$(OBJS)
	$(CC) $^ -o $@ $(LIBS)
%.o:%.c
	$(CC) -c $< $(CCFLAGS)
lfs_format:$(OBJS)
	$(CC) -o $< lfs_format.c
clean:
	rm *.o
