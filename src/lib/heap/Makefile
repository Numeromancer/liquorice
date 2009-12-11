#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = heap-$(arch).o

all: libheap-$(arch).a

libheap-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libheap-$(arch).a $(OBJS)

install: libheap-$(arch).a
	$(INSTALL) libheap-$(arch).a $(LIBDIR)/libheap-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
