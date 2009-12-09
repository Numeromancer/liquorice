#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = udp.o

all: libudp.a

libudp.a: $(OBJS)
	$(AR) $(ARFLAGS) libudp.a $(OBJS)

install: libudp.a
	$(INSTALL) libudp.a $(LIBDIR)/libudp.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
