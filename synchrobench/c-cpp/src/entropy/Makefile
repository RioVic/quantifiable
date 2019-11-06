ROOT = ../..

include $(ROOT)/common/Makefile.common
INCLUDE = $(ROOT)/src/linkedlists/lazy-list/

BINS = $(BINDIR)/$(LOCK)-lazy-list-entropy
CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

main: test.c
	$(MAKE) -C $(ROOT)/src/linkedlists/lazy-list/
	$(CC) $(CFLAGS) -I $(INCLUDE) -L$(BUILDIR)/ test.c -l:$(LOCK)-lazy-list.a -o $(BINS) $(LDFLAGS)

clean:
	rm -f $(BINS)
