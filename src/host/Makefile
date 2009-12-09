SUBDIRS = udptest

all: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done

dos: dummy
	$(MAKE) -C dos all

install: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
	
clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	
clobber: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done

dummy:
