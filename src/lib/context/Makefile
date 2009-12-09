#
# Makefile
#

include ../Makedefs
include ../Makerules

EXTRADIR = $(ARCHDIR)

ifdef CPUDIR
	EXTRADIR := $(EXTRADIR)/$(CPUDIR)
endif

OBJS = $(EXTRADIR)/context_enter.o \
	context.o \
	debug.o \
	isr_context_switch.o \
	spinlock.o

ifeq ($(ARCHDIR),i386)
	OBJS := $(OBJS) i386/trap.o i386/trap_entry.o
endif

all: libcontext.a

libcontext.a: $(OBJS)
	$(AR) $(ARFLAGS) libcontext.a $(OBJS)

install: libcontext.a
	$(INSTALL) libcontext.a $(LIBDIR)/libcontext.a

clean:
	find -name "*.[oas]" -print -exec $(RM) \{\} \;

clobber: clean
	find -name "*~" -print -exec $(RM) \{\} \;
