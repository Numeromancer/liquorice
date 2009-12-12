#
# Makefile
#

include Makedefs
include Makerules

LIBDIR = ../../../lib/i386

SUBDIRS = debug \
	srv

OBJS = main-$(arch).o

all: liquorice.bin

boot: dummy
	$(MAKE) -C boot all

debug: dummy
	$(MAKE) -C debug all

srv: dummy
	$(MAKE) -C srv all

liquorice.bin: liquorice.elf
	$(OBJCOPY) -O binary -S liquorice.elf liquorice.bin

liquorice_libs=libtcp-$(arch).a libudp-$(arch).a \
 libip-$(arch).a libipcsum-$(arch).a \
 libppp_ip-$(arch).a libppp-$(arch).a libppp_ahdlc-$(arch).a \
 libslip-$(arch).a \
 libethernet_ip-$(arch).a libethernet-$(arch).a \
 libip_datalink-$(arch).a libethdev-$(arch).a liboneshot-$(arch).a \
 libsrv-$(arch).a libthread-$(arch).a librwlock-$(arch).a libsem-$(arch).a \
 libcondvar-$(arch).a libnetbuf-$(arch).a libmembuf-$(arch).a \
 libheap-$(arch).a \
 libcontext-$(arch).a libdebug-$(arch).a

LIB_LDFLAGS=$(patsubst lib%.a,-l%,$(liquorice_libs))


#
# Note that "entry.o" *MUST* be the first object in object file list as it
# contains the entry point into the system.  Also note that the libraries must
# be linked in the correct order (i.e. most dependent to least dependent).
#
liquorice.elf: dummy $(OBJS)
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done
	$(LD) $(LDFLAGS) $(LIBDIR)/entry-$(arch).o main-$(arch).o -Lsrv -Ldebug -L$(LIBDIR) \
       $(LIB_LDFLAGS) -lc -lm -o liquorice.elf

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
