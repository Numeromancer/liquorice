#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethernet-$(arch).o

all: libethernet-$(arch).a

libethernet-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libethernet-$(arch).a $(OBJS)

install: libethernet-$(arch).a
	$(INSTALL) libethernet-$(arch).a $(LIBDIR)/libethernet-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
