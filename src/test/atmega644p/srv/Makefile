#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = 3c509-$(arch).o \
	bbi2c-$(arch).o \
	$(ARCHDIR)/bbi2c-$(arch).o \
	ne2000-$(arch).o \
	smc91c96-$(arch).o \
	timer0-$(arch).o \
	uart-$(arch).o

all: libsrv-$(arch).a

libsrv-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libsrv-$(arch).a $(OBJS)

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
