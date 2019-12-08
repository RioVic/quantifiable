ROOT = ../../..

include $(ROOT)/common/Makefile.common

CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

linkedlist-lock.o: 
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/linkedlist-lock.o linkedlist-lock.c

lazy.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/lazy.o lazy.c	 

coupling.o: linkedlist-lock.h linkedlist-lock.o
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/coupling.o coupling.c

intset.o: linkedlist-lock.h coupling.h lazy.h
	$(CC) $(CFLAGS) -c -o $(BUILDIR)/intset.o intset.c

main: linkedlist-lock.o coupling.o lazy.o intset.o
	ar rcs $(BUILDIR)/$(LOCK)-random.a $(BUILDIR)/linkedlist-lock.o $(BUILDIR)/lazy.o $(BUILDIR)/coupling.o $(BUILDIR)/intset.o


clean:
	rm -f $(BINS)
