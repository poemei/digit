CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all test install clean
all: bin/reasoning.so
bin/reasoning.so: src/reasoning.c include/reasoning.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/reasoning.c -o $@
bin/test_reasoning: tests/test_reasoning.c src/reasoning.c include/reasoning.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_reasoning.c src/reasoning.c -o $@
test: bin/test_reasoning
	./bin/test_reasoning
install: all
	install -d -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(PREFIX)/modules/reasoning/bin"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0640 module.conf "$(PREFIX)/modules/reasoning/module.conf"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0750 bin/reasoning.so "$(PREFIX)/modules/reasoning/bin/reasoning.so"
clean:
	rm -rf bin
