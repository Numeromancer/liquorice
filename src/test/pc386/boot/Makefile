include ../Makedefs
include ../Makerules

INCS = -I.

SUBDIRS = linux

all: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done
	$(RM) bootimage
	$(MAKE) bootimage

bootimage: bootsector setup ../liquorice.bin
	linux/build bootsector setup ../liquorice.bin > bootimage

bootsector: bootsector.elf
	$(OBJCOPY) -O binary -S bootsector.elf bootsector

bootsector.elf: bootsector.o
	$(LD) -Ttext=0x0 -s -e _main bootsector.o -o bootsector.elf

setup: setup.elf
	$(OBJCOPY) -O binary -S setup.elf setup

setup.elf: setup.o
	$(LD) -Ttext=0x0 -s -e _main setup.o -o setup.elf

disk: all
	dd if=bootimage of=/dev/fd0 bs=8192

clean:
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	find -name "*.[oas]" -print -exec $(RM) \{\} \;
	$(RM) bootimage
	$(RM) bootsector setup

clobber: clean
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done
	find -name "*.elf" -print -exec $(RM) \{\} \;

dummy:
