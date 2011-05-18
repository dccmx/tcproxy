#
# tcproxy - Makefile
#
# Author: dccmx <dccmx@dccmx.com>
#

PROGNAME   = tcproxy
VERSION    = 0.1.1

OBJFILES   = event.c util.c policy.c
INCFILES   = event.h util.h policy.h

CFLAGS_GEN = -Wall -g $(CFLAGS) -DVERSION=\"$(VERSION)\"
CFLAGS_DBG = -ggdb $(CFLAGS_GEN)
CFLAGS_OPT = -O3 -Wno-format $(CFLAGS_GEN)

LDFLAGS   += 
LIBS      += 

all: $(PROGNAME)

$(PROGNAME): $(PROGNAME).c $(OBJFILES) $(INCFILES)
	$(CC) $(LDFLAGS) $(PROGNAME).c -o $(PROGNAME) $(CFLAGS_OPT) $(OBJFILES) $(LIBS)
	@echo
	@echo "Done! See README for how to use."
	@echo
	@echo "Having problems with it? Send complains and bugs to dccmx@dccmx.com"
	@echo

debug: $(PROGNAME).c $(OBJFILES) $(INCFILES)
	$(CC) $(LDFLAGS) $(PROGNAME).c -o $(PROGNAME) $(CFLAGS_DBG) $(OBJFILES) $(LIBS)

install: $(PROGNAME)
	cp ./$(PROGNAME) /usr/local/bin/

clean:
	rm -f $(PROGNAME) core core.[1-9][0-9]* memcheck.out callgrind.out.[1-9][0-9]* massif.out.[1-9][0-9]*

run: $(PROGNAME)
	rm -f core core.[1-9][0-9]* memcheck.out callgrind.out.[1-9][0-9]* massif.out.[1-9][0-9]*
	./$(PROGNAME) ":11212->:11211"

callgrind: debug
	valgrind --tool=callgrind --collect-systime=yes ./$(PROGNAME) ":11212->:11211"

massif: debug
	valgrind --tool=massif ./$(PROGNAME) ":11212->:11211"

memcheck: debug
	valgrind --leak-check=full --log-file=memcheck.out ./$(PROGNAME) ":11212->:11211"
