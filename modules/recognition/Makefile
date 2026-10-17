CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all test clean install
all: bin/recognition.so
bin/recognition.so: src/recognition.c include/recognition.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -Iinclude -shared src/recognition.c -o $@
bin/test_recognition: tests/test_recognition.c src/recognition.c include/recognition.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -Iinclude tests/test_recognition.c src/recognition.c -o $@
test: bin/test_recognition
	./bin/test_recognition
install: all
	install -d -m 0750 "$(PREFIX)/modules/recognition/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/recognition/module.conf"
	install -m 0750 bin/recognition.so "$(PREFIX)/modules/recognition/bin/recognition.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/recognition"
clean:
	rm -rf bin
