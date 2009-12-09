#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = oneshot.o

all: liboneshot.a

liboneshot.a: $(OBJS)
	$(AR) $(ARFLAGS) liboneshot.a $(OBJS)

install: liboneshot.a
	$(INSTALL) liboneshot.a $(LIBDIR)/liboneshot.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
