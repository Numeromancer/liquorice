#
# Top level Makefile for all of the test configurations.
#

SUBDIRS = mega103 \
	pc386 \
	stk200

all: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done

mega103: dummy
	$(MAKE) -C mega103 all

pc386: dummy
	$(MAKE) -C pc386 all

stk200: dummy
	$(MAKE) -C stk200 all

clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	
clobber: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done

dummy:
