CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -D_DEFAULT_SOURCE -O2
BINS    := manager searcher

.PHONY: all clean

all: $(BINS)

manager: manager.c
	$(CC) $(CFLAGS) -o $@ $<

searcher: searcher.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
