#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp_ahdlc-$(arch).o

all: libppp_ahdlc-$(arch).a

libppp_ahdlc-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libppp_ahdlc-$(arch).a $(OBJS)

install: libppp_ahdlc-$(arch).a
	$(INSTALL) libppp_ahdlc-$(arch).a $(LIBDIR)/libppp_ahdlc-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
