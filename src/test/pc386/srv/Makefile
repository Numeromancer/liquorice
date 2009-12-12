#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = 3c509-$(arch).o \
	i82595-$(arch).o \
	ne2000-$(arch).o \
	pic-$(arch).o \
	timer-$(arch).o \
	uart-$(arch).o

all: libsrv-$(arch).a

libsrv-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libsrv-$(arch).a $(OBJS)

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
