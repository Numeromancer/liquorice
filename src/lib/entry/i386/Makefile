#
# Makefile
#

include ../../Makedefs
include ../../Makerules

ENTRYOBJ = entry-$(arch).o

all: $(ENTRYOBJ)

install: $(ENTRYOBJ)
	$(INSTALL) $(ENTRYOBJ) $(LIBDIR)/entry-$(arch).o

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
