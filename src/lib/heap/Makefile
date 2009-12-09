#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = heap.o

all: libheap.a

libheap.a: $(OBJS)
	$(AR) $(ARFLAGS) libheap.a $(OBJS)

install: libheap.a
	$(INSTALL) libheap.a $(LIBDIR)/libheap.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
