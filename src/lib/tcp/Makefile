#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = tcp-$(arch).o

all: libtcp-$(arch).a

libtcp-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libtcp-$(arch).a $(OBJS)

install: libtcp-$(arch).a
	$(INSTALL) libtcp-$(arch).a $(LIBDIR)/libtcp-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
