CC = gcc
CFLAGS = -O3 -Wall -I..
CP = cp -av
RM = rm -f
STRIP = strip

.c.o:
	$(CC) $(CFLAGS) -c $*.c

all: build

build: build.o
	$(CC) -o build build.o

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	$(RM) build

dummy:
