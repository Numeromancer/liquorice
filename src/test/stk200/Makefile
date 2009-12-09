#
# Makefile
#

include Makedefs
include Makerules

LIBDIR = ../../../lib/avr/at90s8515

SUBDIRS = debug

OBJS = main.o

all: liquorice.srec

debug: dummy
	$(MAKE) -C debug all

liquorice.srec: liquorice.elf
	$(OBJCOPY) -O srec liquorice.elf liquorice.srec

#
# Note that "entry.o" *MUST* be the first object in object file list as it
# contains the entry point into the system.  Also note that the libraries must
# be linked in the correct order (i.e. most dependent to least dependent).
#
liquorice.elf: dummy $(OBJS)
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done
	$(LD) $(LDFLAGS) $(LIBDIR)/entry.o main.o -Ldebug -L$(LIBDIR) -L/usr/local/lib/gcc-lib/avr/2.95.2 -lthread -lrwlock -lsem -lcondvar -lheap -lcontext -ldebug -lgcc -o liquorice.elf

reset:
	uisp -dstk200
	
erase:
	uisp -dstk200 --erase

install: liquorice.elf
	uisp -dstk200 --upload if=liquorice.srec -v=3

reload: erase install
	
clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	find -name "*.[oas]" -print -exec $(RM) \{\} \;
	
clobber: clean
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done
	find -name "*.srec" -print -exec $(RM) \{\} \;
	find -name "*.elf" -print -exec $(RM) \{\} \;
	find -name "*~" -print -exec $(RM) \{\} \;

dummy:
