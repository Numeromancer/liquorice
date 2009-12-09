#
# Makefile
#

include ../../Makedefs
include ../../Makerules

ENTRYOBJ = entry.o

all: $(ENTRYOBJ)

install: $(ENTRYOBJ)
	$(INSTALL) $(ENTRYOBJ) $(LIBDIR)/entry.o

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
