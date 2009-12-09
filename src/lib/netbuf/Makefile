#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = netbuf.o

all: libnetbuf.a

libnetbuf.a: $(OBJS)
	$(AR) $(ARFLAGS) libnetbuf.a $(OBJS)

install: libnetbuf.a
	$(INSTALL) libnetbuf.a $(LIBDIR)/libnetbuf.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
