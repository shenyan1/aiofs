CC    = gcc
PROGS = client
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
OBJ2S = ../lfs_sys.o xfstest.o
OBJS  += ../lfs_sys.o
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread -laio
all: 
%.o:%.c
	$(CC) -o xfstest -c $< $(CCFLAGS)
clean:
	rm *.o
