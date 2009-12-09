#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp_ip.o

all: libppp_ip.a

libppp_ip.a: $(OBJS)
	$(AR) $(ARFLAGS) libppp_ip.a $(OBJS)

install: libppp_ip.a
	$(INSTALL) libppp_ip.a $(LIBDIR)/libppp_ip.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
