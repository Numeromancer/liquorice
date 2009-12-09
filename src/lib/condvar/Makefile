#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = condvar.o

all: libcondvar.a

libcondvar.a: $(OBJS)
	$(AR) $(ARFLAGS) libcondvar.a $(OBJS)

install: libcondvar.a
	$(INSTALL) libcondvar.a $(LIBDIR)/libcondvar.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
