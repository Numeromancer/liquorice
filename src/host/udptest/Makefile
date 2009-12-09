include Makerules

LIBS = -lc

OBJS = main.o

all: udptest

udptest: $(OBJS)
	$(CC) -o udptest $(OBJS) $(LIBS)

.PHONY: install clobber
	
install: all
#	$(STRIP) udptest
#	$(CP) udptest ../../../bin

clobber: clean
	$(RM) udptest
	find -name ".depend" -print -exec $(RM) \{\} \;

