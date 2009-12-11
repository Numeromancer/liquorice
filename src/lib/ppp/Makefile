#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp-$(arch).o

all: libppp-$(arch).a

libppp-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

install: libppp-$(arch).a
	$(INSTALL) libppp-$(arch).a $(LIBDIR)/libppp-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
