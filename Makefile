override CFLAGS += -O3 -pthread -Wno-attributes -m64
CC=gcc

#BINARIES=test kaslr physical_reader

SOURCES := $(wildcard *.c)
BINARIES := $(SOURCES:%.c=%)

all: $(BINARIES)

libkdump/libkdump.a:  libkdump/libkdump.c
	make -C libkdump

%: %.c libkdump/libkdump.a
	$(CC) $< -o $@ -m64 -Llibkdump -Ilibkdump -lkdump -static $(CFLAGS)
	
	
clean:
	rm -f *.o $(BINARIES)
	make clean -C libkdump
