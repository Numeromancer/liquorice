#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = slip-$(arch).o

all: libslip-$(arch).a

libslip-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libslip-$(arch).a $(OBJS)

install: libslip-$(arch).a
	$(INSTALL) libslip-$(arch).a $(LIBDIR)/libslip-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
