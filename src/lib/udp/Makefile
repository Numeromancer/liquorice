#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = udp-$(arch).o

all: libudp-$(arch).a

libudp-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libudp-$(arch).a $(OBJS)

install: libudp-$(arch).a
	$(INSTALL) libudp-$(arch).a $(LIBDIR)/libudp-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
