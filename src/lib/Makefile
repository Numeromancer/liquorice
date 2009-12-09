#
# Top level makefile for the various Liquorice libraries.
#

include Makedefs
include Makerules

SUBDIRS = condvar \
	context \
	entry \
	ethdev \
	ethernet \
	ethernet_ip \
	heap \
	ip \
	ip_datalink \
	ipcsum \
	membuf \
	netbuf \
	oneshot \
	ppp \
	ppp_ahdlc \
	ppp_ip \
	rwlock \
	sem \
	slip \
	tcp \
	thread \
	udp

all: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i all; done

condvar: dummy
	$(MAKE) -C condvar all

context: dummy
	$(MAKE) -C context all

entry: dummy
	$(MAKE) -C entry all

ethdev: dummy
	$(MAKE) -C ethdev all

ethernet: dummy
	$(MAKE) -C ethernet all

ethernet_ip: dummy
	$(MAKE) -C ethernet_ip all

heap: dummy
	$(MAKE) -C heap all

ip: dummy
	$(MAKE) -C ip all

ip_datalink: dummy
	$(MAKE) -C ip_datalink all

ipcsum: dummy
	$(MAKE) -C ipcsum all

membuf: dummy
	$(MAKE) -C membuf all

netbuf: dummy
	$(MAKE) -C netbuf all

oneshot: dummy
	$(MAKE) -C oneshot all

ppp: dummy
	$(MAKE) -C ppp all

ppp_ahdlc: dummy
	$(MAKE) -C ppp_ahdlc all

ppp_ip: dummy
	$(MAKE) -C ppp_ip all

rwlock: dummy
	$(MAKE) -C rwlock all

sem: dummy
	$(MAKE) -C sem all

slip: dummy
	$(MAKE) -C slip all

tcp: dummy
	$(MAKE) -C tcp all

thread: dummy
	$(MAKE) -C thread all

udp: dummy
	$(MAKE) -C udp all

install: all
	for i in $(SUBDIRS); do $(MAKE) -C $$i install; done
	
clean: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clean; done
	
clobber: dummy
	for i in $(SUBDIRS); do $(MAKE) -C $$i clobber; done

dummy:
