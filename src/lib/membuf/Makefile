#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = membuf.o

all: libmembuf.a

libmembuf.a: $(OBJS)
	$(AR) $(ARFLAGS) libmembuf.a $(OBJS)

install: libmembuf.a
	$(INSTALL) libmembuf.a $(LIBDIR)/libmembuf.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
