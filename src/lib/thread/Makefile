#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = thread-$(arch).o

all: libthread-$(arch).a

libthread-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libthread-$(arch).a $(OBJS)

install: libthread-$(arch).a
	$(INSTALL) libthread-$(arch).a $(LIBDIR)/libthread-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
