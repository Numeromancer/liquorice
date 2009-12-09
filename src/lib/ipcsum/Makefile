#
# Makefile
#

include ../Makedefs
include ../Makerules

EXTRADIR = $(ARCHDIR)

OBJS = ipcsum.o \
	$(EXTRADIR)/ipcsum.o

all: libipcsum.a

libipcsum.a: $(OBJS)
	$(AR) $(ARFLAGS) libipcsum.a $(OBJS)

install: libipcsum.a
	$(INSTALL) libipcsum.a $(LIBDIR)/libipcsum.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
