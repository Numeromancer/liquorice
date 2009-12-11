#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = netbuf-$(arch).o

all: libnetbuf-$(arch).a

libnetbuf-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libnetbuf-$(arch).a $(OBJS)

install: libnetbuf-$(arch).a
	$(INSTALL) libnetbuf-$(arch).a $(LIBDIR)/libnetbuf-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
