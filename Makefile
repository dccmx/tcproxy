#
# tcproxy - Makefile
#
# Author: dccmx <dccmx@dccmx.com>
#

PROGNAME   = tcproxy
VERSION    = 0.1.1

OBJFILES   = event.c util.c policy.c
INCFILES   = event.h util.h policy.h

CFLAGS_GEN = -Wall -g -funsigned-char $(CFLAGS) -DVERSION=\"$(VERSION)\"
CFLAGS_DBG = -g -ggdb $(CFLAGS_GEN)
CFLAGS_OPT = -O3 -Wno-format $(CFLAGS_GEN)

LDFLAGS   += 
LIBS      += 

all: $(PROGNAME)

$(PROGNAME): $(PROGNAME).c $(OBJFILES) $(INCFILES)
	$(CC) $(LDFLAGS) $(PROGNAME).c -o $(PROGNAME) $(CFLAGS_OPT) $(OBJFILES) $(LIBS)
	@echo
	@echo "See README for how to use."
	@echo
	@echo "Having problems with it? Send complains to dccmx@dccmx.com"
	@echo

debug: $(PROGNAME).c $(OBJFILES) $(INCFILES)
	$(CC) $(LDFLAGS) $(PROGNAME).c -o $(PROGNAME) $(CFLAGS_DBG) $(OBJFILES) $(LIBS)

install: $(PROGNAME)
	cp ./$(PROGNAME) /usr/local/bin/

run: $(PROGNAME)
	rm -rf core.*
	./$(PROGNAME) ":11212->:11211"

callgrind: debug
	valgrind --tool=callgrind --collect-systime=yes ./$(PROGNAME) ":11212->:11211"

massif: debug
	valgrind --tool=massif ./$(PROGNAME) ":11212->:11211"

memcheck: debug
	valgrind --leak-check=full --log-file=memcheck.out ./$(PROGNAME) ":11212->:11211"

clean:
	rm -f $(PROGNAME) core core.[1-9][0-9]* memcheck.out callgrind.out.* massif.out.*

dist: clean
	cd ..; rm -rf $(PROGNAME)-$(VERSION); cp -pr $(PROGNAME) $(PROGNAME)-$(VERSION); \
	  tar cfvz ./$(PROGNAME)-$(VERSION).tar.gz $(PROGNAME)-$(VERSION)
	cd ..; rm -rf $(PROGNAME)-$(VERSION); chmod 644 ./$(PROGNAME)-$(VERSION).tar.gz

