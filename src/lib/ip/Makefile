#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ip-$(arch).o

all: libip-$(arch).a

libip-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libip-$(arch).a $(OBJS)

install: libip-$(arch).a
	$(INSTALL) libip-$(arch).a $(LIBDIR)/libip-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
