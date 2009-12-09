#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = sem.o

all: libsem.a

libsem.a: $(OBJS)
	$(AR) $(ARFLAGS) libsem.a $(OBJS)

install: libsem.a
	$(INSTALL) libsem.a $(LIBDIR)/libsem.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
