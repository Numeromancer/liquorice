#
# Makefile
#

include Makedefs
include Makerules

LIBDIR = ../../../lib/i386

SUBDIRS = debug \
	srv

OBJS = main.o

all: liquorice.bin

boot: dummy
	$(MAKE) -C boot all

debug: dummy
	$(MAKE) -C debug all

srv: dummy
	$(MAKE) -C srv all

liquorice.bin: liquorice.elf
	$(OBJCOPY) -O binary -S liquorice.elf liquorice.bin

#
# Note that "entry.o" *MUST* be the first object in object file list as it
# contains the entry point into the system.  Also note that the libraries must
# be linked in the correct order (i.e. most dependent to least dependent).
#
liquorice.elf: dummy $(OBJS)
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done
	$(LD) $(LDFLAGS) $(LIBDIR)/entry.o main.o -Lsrv -Ldebug -L$(LIBDIR) -L/usr/lib/gcc-lib/i386-linux/egcs-2.91.66 \
		-ltcp -ludp -lip -lipcsum -lppp_ip -lppp -lppp_ahdlc -lslip \
		-lethernet_ip -lethernet -lip_datalink -lethdev -loneshot \
		-lsrv -lthread -lrwlock -lsem -lcondvar -lnetbuf -lmembuf \
		-lheap -lcontext -ldebug -lgcc -o liquorice.elf
	
disk: all
	$(MAKE) -C boot disk
	
clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	find -name "*.[oas]" -print -exec $(RM) \{\} \;
	
clobber: clean
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done
	find -name "*.bin" -print -exec $(RM) \{\} \;
	find -name "*.elf" -print -exec $(RM) \{\} \;
	find -name "*~" -print -exec $(RM) \{\} \;

dummy:
