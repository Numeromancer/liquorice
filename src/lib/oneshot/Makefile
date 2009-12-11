#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = oneshot-$(arch).o

all: liboneshot-$(arch).a

liboneshot-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) liboneshot-$(arch).a $(OBJS)

install: liboneshot-$(arch).a
	$(INSTALL) liboneshot-$(arch).a $(LIBDIR)/liboneshot-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
