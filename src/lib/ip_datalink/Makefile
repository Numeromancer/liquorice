#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ip_datalink-$(arch).o

all: libip_datalink-$(arch).a

libip_datalink-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libip_datalink-$(arch).a $(OBJS)

install: libip_datalink-$(arch).a
	$(INSTALL) libip_datalink-$(arch).a $(LIBDIR)/libip_datalink-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
