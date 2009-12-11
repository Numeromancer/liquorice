#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethernet_ip-$(arch).o

all: libethernet_ip-$(arch).a

libethernet_ip-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libethernet_ip-$(arch).a $(OBJS)

install: libethernet_ip.a
	$(INSTALL) libethernet_ip-$(arch).a $(LIBDIR)/libethernet_ip-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
