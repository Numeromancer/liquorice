#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = condvar-$(arch).o

all: libcondvar-$(arch).a

libcondvar-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libcondvar-$(arch).a $(OBJS)

install: libcondvar-$(arch).a
	$(INSTALL) libcondvar-$(arch).a $(LIBDIR)/libcondvar-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
