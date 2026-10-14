CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all test clean install
all: bin/interpretation.so
bin/interpretation.so: src/interpretation.c $(ROOT)/include/interpretation.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared src/interpretation.c -o $@
bin/test_interpretation: tests/test_interpretation.c src/interpretation.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include tests/test_interpretation.c src/interpretation.c -o $@
test: bin/test_interpretation
	./bin/test_interpretation
install: all
	install -d -m 0750 "$(PREFIX)/modules/interpretation/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/interpretation/module.conf"
	install -m 0750 bin/interpretation.so "$(PREFIX)/modules/interpretation/bin/interpretation.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/interpretation"
clean:
	rm -rf bin
