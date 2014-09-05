CC    = gcc
PROGS = client
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
OBJ1S = lfs_format.o lfs_format_dir.o lfs_formatfile.o ../lfs_sys.o
OBJ2S = ../lfs_sys.o iotool.o rfsio.o 
OBJS  += ../lfs_sys.o
CCFLAGS = -g -Wall -O0
LIBS  = -lpthread -laio
lfs_format:$(OBJ1S)
	$(CC) -o $@ $^
all: iotool
iotool:$(OBJ2S)
	$(CC) $^ -g -o $@ $(LIBS)
%.o:%.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm *.o
