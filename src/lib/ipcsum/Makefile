#
# Makefile
#

include ../Makedefs
include ../Makerules

EXTRADIR = $(ARCHDIR)

OBJS = ipcsum-$(arch).o \
	$(EXTRADIR)/ipcsum-$(arch).o

all: libipcsum-$(arch).a

libipcsum-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libipcsum-$(arch).a $(OBJS)

install: libipcsum-$(arch).a
	$(INSTALL) libipcsum-$(arch).a $(LIBDIR)/libipcsum-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
