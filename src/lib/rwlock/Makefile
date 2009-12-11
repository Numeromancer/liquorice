#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = rwlock-$(arch).o

all: librwlock-$(arch).a

librwlock-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) librwlock-$(arch).a $(OBJS)

install: librwlock-$(arch).a
	$(INSTALL) librwlock-$(arch).a $(LIBDIR)/librwlock-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
