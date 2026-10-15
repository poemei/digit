CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC

# Resolve every module file relative to this Makefile, not the caller's cwd.
MODULE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(MODULE_DIR)../..)
TARGET := $(MODULE_DIR)bin/irc.so
TEST_TARGET := $(MODULE_DIR)bin/test_irc
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit

.PHONY: all clean test install
all: $(TARGET)

$(TARGET): $(MODULE_DIR)src/irc.c
	mkdir -p "$(MODULE_DIR)bin"
	$(CC) $(CFLAGS) -I"$(ROOT)/include" -shared "$<" -o "$@" -lssl -lcrypto -lpthread

$(TEST_TARGET): $(MODULE_DIR)tests/test_irc.c $(MODULE_DIR)src/irc.c
	mkdir -p "$(MODULE_DIR)bin"
	$(CC) $(CFLAGS) -I"$(ROOT)/include" "$(MODULE_DIR)tests/test_irc.c" -o "$@" -lssl -lcrypto -lpthread

test: $(TARGET) $(TEST_TARGET)
	"$(TEST_TARGET)"

install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/irc" "$(PREFIX)/modules/irc/bin"
	install -m 0640 "$(MODULE_DIR)module.conf" "$(PREFIX)/modules/irc/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/irc/bin/irc.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/irc"

clean:
	rm -rf "$(MODULE_DIR)bin"
