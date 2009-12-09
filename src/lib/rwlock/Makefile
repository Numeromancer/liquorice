#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = rwlock.o

all: librwlock.a

librwlock.a: $(OBJS)
	$(AR) $(ARFLAGS) librwlock.a $(OBJS)

install: librwlock.a
	$(INSTALL) librwlock.a $(LIBDIR)/librwlock.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
