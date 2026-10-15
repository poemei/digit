CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/irc.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/irc.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared src/irc.c -o $@ -lssl -lcrypto -lpthread
bin/test_irc: tests/test_irc.c src/irc.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include tests/test_irc.c -o $@ -lssl -lcrypto -lpthread
test: $(TARGET) bin/test_irc
	./bin/test_irc
install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/irc" "$(PREFIX)/modules/irc/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/irc/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/irc/bin/irc.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/irc"
clean:
	rm -rf bin
