#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ip.o

all: libip.a

libip.a: $(OBJS)
	$(AR) $(ARFLAGS) libip.a $(OBJS)

install: libip.a
	$(INSTALL) libip.a $(LIBDIR)/libip.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
