#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = membuf-$(arch).o

all: libmembuf-$(arch).a

libmembuf-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libmembuf-$(arch).a $(OBJS)

install: libmembuf-$(arch).a
	$(INSTALL) libmembuf-$(arch).a $(LIBDIR)/libmembuf-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
