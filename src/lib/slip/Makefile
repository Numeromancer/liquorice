#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = slip.o

all: libslip.a

libslip.a: $(OBJS)
	$(AR) $(ARFLAGS) libslip.a $(OBJS)

install: libslip.a
	$(INSTALL) libslip.a $(LIBDIR)/libslip.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
