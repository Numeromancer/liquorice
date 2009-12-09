#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp.o

all: libppp.a

libppp.a: $(OBJS)
	$(AR) $(ARFLAGS) libppp.a $(OBJS)

install: libppp.a
	$(INSTALL) libppp.a $(LIBDIR)/libppp.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
