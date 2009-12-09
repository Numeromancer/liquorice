#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethdev.o

all: libethdev.a

libethdev.a: $(OBJS)
	$(AR) $(ARFLAGS) libethdev.a $(OBJS)

install: libethdev.a
	$(INSTALL) libethdev.a $(LIBDIR)/libethdev.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
