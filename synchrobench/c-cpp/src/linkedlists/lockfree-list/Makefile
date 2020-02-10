ROOT = ../../..

include $(ROOT)/common/Makefile.common

ifeq ($(STM),SEQUENTIAL)
  BINS = $(BINDIR)/sequential-linkedlist
else ifeq ($(STM),LOCKFREE)
  BINS = $(BINDIR)/lockfree-linkedlist
else
  BINS = $(BINDIR)/$(STM)-linkedlist
endif
CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

linkedlist.o:
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist.o linkedlist.c

harris.o: linkedlist.h linkedlist.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/harris.o harris.c

intset.o: linkedlist.h harris.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

test.o: linkedlist.h harris.h intset.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/test.o test.c

options.o: testutils.h intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/options.o options.c

testutils.o: intset.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/testutils.o testutils.c

main: linkedlist.o harris.o intset.o test.o $(TMILB) options.o testutils.o
	$(CC) $(CFLAGS) $(BUILDIR)/linkedlist.o $(BUILDIR)/harris.o $(BUILDIR)/intset.o $(BUILDIR)/test.o $(BUILDIR)/options.o $(BUILDIR)/testutils.o main.c -o $(BINS) $(LDFLAGS)

clean:
	-rm -f $(BINS)
