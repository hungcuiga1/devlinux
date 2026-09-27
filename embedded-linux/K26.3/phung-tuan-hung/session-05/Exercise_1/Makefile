CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -D_DEFAULT_SOURCE -O2
TARGET  := manager

.PHONY: all clean

all: $(TARGET)

$(TARGET): manager.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
