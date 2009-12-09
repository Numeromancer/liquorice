#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = ethernet.o

all: libethernet.a

libethernet.a: $(OBJS)
	$(AR) $(ARFLAGS) libethernet.a $(OBJS)

install: libethernet.a
	$(INSTALL) libethernet.a $(LIBDIR)/libethernet.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
