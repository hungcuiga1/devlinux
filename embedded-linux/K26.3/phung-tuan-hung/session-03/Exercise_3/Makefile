CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -O2
BINS    := writer_flock writer_fcntl
LOG     := system.log
# number of concurrent processes in tests
N       ?= 50

.PHONY: all test_flock test_fcntl clean

all: $(BINS)

writer_flock: writer_flock.c
	$(CC) $(CFLAGS) -o $@ $<

writer_fcntl: writer_fcntl.c
	$(CC) $(CFLAGS) -o $@ $<

# Spawn N processes at the same time, wait for all, then verify
# line count and format of every line.
define run_test
	@rm -f $(LOG)
	@echo ">>> Launching $(N) concurrent $(1) processes..."
	@for i in $$(seq 1 $(N)); do ./$(1) "message $$i from $(1)" & done; wait
	@lines=$$(wc -l < $(LOG)); \
	 good=$$(grep -cE '^\[PID:[0-9]+\] \[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] \[INFO\] message [0-9]+ from $(1)$$' $(LOG)); \
	 echo "    lines written : $$lines / $(N)"; \
	 echo "    valid format  : $$good / $(N)"; \
	 head -n 3 $(LOG) | sed 's/^/    /'; \
	 if [ "$$lines" -eq "$(N)" ] && [ "$$good" -eq "$(N)" ]; then echo "    RESULT: PASS"; \
	 else echo "    RESULT: FAIL"; exit 1; fi
endef

test_flock: writer_flock
	$(call run_test,writer_flock)

test_fcntl: writer_fcntl
	$(call run_test,writer_fcntl)

clean:
	rm -f $(BINS) $(LOG)
