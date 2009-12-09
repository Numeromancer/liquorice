#
# Normally I like the order in which things get built to be alphabetical,
# but in this case we must build the code in order of subsequent dependencies.
# For now of course this is alphabetical order ;-)
#

SUBDIRS = host \
	lib

all: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done

host: dummy
	$(MAKE) -C host all

lib: dummy
	$(MAKE) -C lib all

install: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
	
clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	
clobber: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done

dummy:
