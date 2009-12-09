#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ip_datalink.o

all: libip_datalink.a

libip_datalink.a: $(OBJS)
	$(AR) $(ARFLAGS) libip_datalink.a $(OBJS)

install: libip_datalink.a
	$(INSTALL) libip_datalink.a $(LIBDIR)/libip_datalink.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
