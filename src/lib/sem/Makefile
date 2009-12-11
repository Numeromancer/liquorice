#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = sem-$(arch).o

all: libsem-$(arch).a

libsem-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libsem-$(arch).a $(OBJS)

install: libsem-$(arch).a
	$(INSTALL) libsem-$(arch).a $(LIBDIR)/libsem-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
