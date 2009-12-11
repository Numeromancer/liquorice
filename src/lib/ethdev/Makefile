#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethdev-$(arch).o

all: libethdev-$(arch).a

libethdev-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libethdev-$(arch).a $(OBJS)

install: libethdev.a
	$(INSTALL) libethdev-$(arch).a $(LIBDIR)/libethdev-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
