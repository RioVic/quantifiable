ROOT = ../..

# Change these to configure which item to build

# lazy-list, hoh-list, null
ifeq ($(ARTIFACT),)
	ARTIFACT = hoh-list
endif

# ll/lazy-list, ll/lock-coupling-list, pure-random, etc...
ifeq ($(SUBJECT_DIR),)
	SUBJECT_DIR = linkedlists/lock-coupling-list
endif

# =====================

include $(ROOT)/common/Makefile.common
INCLUDE = $(ROOT)/src/$(SUBJECT_DIR)

BINS = $(BINDIR)/$(LOCK)-$(ARTIFACT)-entropy
CFLAGS += -std=gnu89

.PHONY:	all clean

all:	main

main: test.c testutils.h testutils.c options.c options.h
	$(MAKE) -C $(ROOT)/src/$(SUBJECT_DIR)
	$(CC) $(CFLAGS) -I $(INCLUDE) -L$(BUILDIR)/ main.c test.c testutils.c options.c -l:$(LOCK)-$(ARTIFACT).a -o $(BINS) $(LDFLAGS) 

clean:
	rm -f $(BINS)
