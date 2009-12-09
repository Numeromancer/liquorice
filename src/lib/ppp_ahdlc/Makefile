#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ppp_ahdlc.o

all: libppp_ahdlc.a

libppp_ahdlc.a: $(OBJS)
	$(AR) $(ARFLAGS) libppp_ahdlc.a $(OBJS)

install: libppp_ahdlc.a
	$(INSTALL) libppp_ahdlc.a $(LIBDIR)/libppp_ahdlc.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
