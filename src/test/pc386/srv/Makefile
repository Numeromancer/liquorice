#
# Makefile
#

include ../Makedefs
include ../Makerules

OBJS = 3c509.o \
	i82595.o \
	ne2000.o \
	pic.o \
	timer.o \
	uart.o

all: libsrv.a

libsrv.a: $(OBJS)
	$(AR) $(ARFLAGS) libsrv.a $(OBJS)

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
