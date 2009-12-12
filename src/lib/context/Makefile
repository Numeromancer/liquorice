#
# Makefile
#

include ../Makedefs
include ../Makerules

EXTRADIR = $(ARCHDIR)

ifdef CPUDIR
	EXTRADIR := $(EXTRADIR)/$(CPUDIR)
endif

OBJS = $(EXTRADIR)/context_enter-$(arch).o \
	context-$(arch).o \
	debug-$(arch).o \
	isr_context_switch-$(arch).o \
	spinlock-$(arch).o

ifeq ($(ARCHDIR),i386)
	OBJS := $(OBJS) i386/trap-$(arch).o i386/trap_entry-$(arch).o
endif

all: libcontext-$(arch).a

libcontext-$(arch).a: $(OBJS)
	$(AR) $(ARFLAGS) libcontext-$(arch).a $(OBJS)

install: libcontext-$(arch).a
	$(INSTALL) libcontext-$(arch).a $(LIBDIR)/libcontext-$(arch).a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
