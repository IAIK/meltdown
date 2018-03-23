override CFLAGS += -O3 -pthread -Wno-attributes
CC=gcc

all: libkdump.a libkdump.so

libkdump.o: libkdump.c
	$(CC) -c -m64 -fPIC libkdump.c -o libkdump.o $(CFLAGS)
	
libkdump.so: libkdump.o
	$(CC) -shared -m64 -Wl,-soname,libkdump.so -o libkdump.so libkdump.o $(CFLAGS)
	
libkdump.a: libkdump.o
	ar rcs libkdump.a libkdump.o 

install: libkdump.a libkdump.so
	cp libkdump.so /usr/local/lib
	cp libkdump.h /usr/local/include
	ldconfig
	
uninstall: 
	rm /usr/local/lib/libkdump.so
	rm /usr/local/include/libkdump.h
	ldconfig
	
clean:
	rm -f *.o *.so *.a
