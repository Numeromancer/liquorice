#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = debug-$(arch).o

all: libdebug-$(arch).a

libdebug-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libdebug-$(arch).a $(OBJS)

install: libdebug-$(arch).a
	$(INSTALL) libdebug-$(arch).a $(LIBDIR)/libdebug-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
