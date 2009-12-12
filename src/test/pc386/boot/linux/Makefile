include ../../Makerules
include ../../Makedefs

CC = gcc
CFLAGS = -O3 -Wall -I..
CP = cp -av
RM = rm -f
STRIP = strip

all: build

build: build-$(arch).o
	$(CC) -o build build-$(arch).o

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	$(RM) build

dummy:
