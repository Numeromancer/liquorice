#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp_ip-$(arch).o

all: libppp_ip-$(arch).a

libppp_ip-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libppp_ip-$(arch).a $(OBJS)

install: libppp_ip-$(arch).a
	$(INSTALL) libppp_ip-$(arch).a $(LIBDIR)/libppp_ip-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
