#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = debug.o

all: libdebug.a

libdebug.a: $(OBJS)
	$(AR) $(ARFLAGS) libdebug.a $(OBJS)

install: libdebug.a
	$(INSTALL) libdebug.a $(LIBDIR)/libdebug.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
