#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = thread.o

all: libthread.a

libthread.a: $(OBJS)
	$(AR) $(ARFLAGS) libthread.a $(OBJS)

install: libthread.a
	$(INSTALL) libthread.a $(LIBDIR)/libthread.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
