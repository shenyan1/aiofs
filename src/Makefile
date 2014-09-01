CC    = gcc
PROGS = lfs_load
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
OBJS += client/rfsio.o
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread -laio
all: lfs_load
lfs_load:$(OBJS)
	$(CC) $^ -g -o $@ $(LIBS)
%.o:%.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm *.o
