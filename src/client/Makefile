CC    = gcc
PROGS = client
SRCS  = $(wildcard *.c)
OBJS  = $(SRCS:.c=.o)
LIBNAME = librfsio.so
CFLAGS := -Wall -I. -g -O2 -fPIC
SO_CFLAGS= -shared $(CFLAGS)
OBJ1S = lfs_format.o lfs_format_dir.o lfs_formatfile.o
OBJ2S = iotool.o rfsio.o 
SOOBJ = rfsio.o
CCFLAGS = -g -Wall -O0 -fomit-frame-pointer -fPIC
LIBS  = -lpthread -laio -lrfsio
lfs_format:$(OBJ1S)
	$(CC) -o $@ $^
all: rfsio install iotool

rfsio:
	$(CC) $(CFLAGS) -c rfsio.c
	$(CC) $(SO_CFLAGS) -o $(LIBNAME) rfsio.o

install:
	install -D -m 755 $(LIBNAME) /usr/lib/$(LIBNAME)
#	ln -sf $(LIBNAME) /usr/lib/

iotool:$(OBJ2S)
	$(CC) iotool.c -g -o iotool $(LIBS)

%.o:%.c
	$(CC) -c $< $(CCFLAGS)

clean:
	rm *.o

