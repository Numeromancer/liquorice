#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethernet_ip.o

all: libethernet_ip.a

libethernet_ip.a: $(OBJS)
	$(AR) $(ARFLAGS) libethernet_ip.a $(OBJS)

install: libethernet_ip.a
	$(INSTALL) libethernet_ip.a $(LIBDIR)/libethernet_ip.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
