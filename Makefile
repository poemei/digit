CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude

.PHONY: all test clean
all: bin/test_agent_root

bin:
	mkdir -p bin

bin/test_agent_root: tests/test_agent_root.c src/agent_root.c src/identity_digit.c include/agent_root.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_agent_root.c src/agent_root.c src/identity_digit.c -o $@

test: all
	./bin/test_agent_root

clean:
	rm -rf bin
