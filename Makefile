CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude

.PHONY: all test clean
all: bin/digit bin/test_agent_root bin/test_runtime bin/test_module_manager bin/test_logging

bin:
	mkdir -p bin

bin/digit: src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/module_manager.c src/logging.c include/logging.h include/digit.h include/runtime.h include/agent_root.h include/module_manager.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/module_manager.c src/logging.c -o $@

bin/test_agent_root: tests/test_agent_root.c src/agent_root.c src/identity_digit.c include/agent_root.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_agent_root.c src/agent_root.c src/identity_digit.c -o $@

bin/test_runtime: tests/test_runtime.c src/runtime.c src/logging.c include/runtime.h include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_runtime.c src/runtime.c src/logging.c -o $@

bin/test_module_manager: tests/test_module_manager.c src/module_manager.c src/logging.c include/module_manager.h include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_module_manager.c src/module_manager.c src/logging.c -o $@

bin/test_logging: tests/test_logging.c src/logging.c include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_logging.c src/logging.c -o $@

test: all
	@sh tests/run_all.sh

clean:
	rm -rf bin
