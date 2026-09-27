CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -D_DEFAULT_SOURCE -O2 -pthread
LDFLAGS := -pthread
TARGET  := print_queue

.PHONY: all clean

all: $(TARGET)

$(TARGET): print_queue.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
