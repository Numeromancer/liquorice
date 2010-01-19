#
# Makefile
#

include Makedefs
include Makerules

LIBDIR = ../../../lib/avr/atmega644p

SUBDIRS = debug srv

OBJS = main-$(arch).o

all: liquorice.elf

debug: dummy
	$(MAKE) -C debug all

srv: dummy
	$(MAKE) -C srv all

liquorice.srec: liquorice.elf
	$(OBJCOPY) -O srec liquorice.elf liquorice.srec

liquorice_libs= libtcp-$(arch).a libudp-$(arch).a libip-$(arch).a libipcsum-$(arch).a \
 libppp_ip-$(arch).a libppp-$(arch).a libppp_ahdlc-$(arch).a libslip-$(arch).a \
 libethernet_ip-$(arch).a libethernet-$(arch).a libip_datalink-$(arch).a \
 libethdev-$(arch).a liboneshot-$(arch).a \
 libthread-$(arch).a librwlock-$(arch).a libsem-$(arch).a \
 libcondvar-$(arch).a libnetbuf-$(arch).a libmembuf-$(arch).a \
 libheap-$(arch).a libcontext-$(arch).a \
 libdebug-$(arch).a libsrv-$(arch).a 

TARGET_LDFLAGS=

LIB_LDFLAGS=$(patsubst lib%.a,-l%,$(liquorice_libs))
#
# Note that "entry.o" *MUST* be the first object in object file list as it
# contains the entry point into the system.  Also note that the libraries must
# be linked in the correct order (i.e. most dependent to least dependent).
#
liquorice.elf: dummy $(OBJS)
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done
	$(LD) $(LIBDIR)/entry-$(arch).o main-$(arch).o \
       -L/usr/avr/lib -Ldebug -Lsrv -L$(LIBDIR) \
       $(LIB_LDFLAGS) -lm -lc \
       -o liquorice.elf \
       $(TARGET_LDFLAGS) $(LDFLAGS)

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
