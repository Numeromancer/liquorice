#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = tcp.o

all: libtcp.a

libtcp.a: $(OBJS)
	$(AR) $(ARFLAGS) libtcp.a $(OBJS)

install: libtcp.a
	$(INSTALL) libtcp.a $(LIBDIR)/libtcp.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
