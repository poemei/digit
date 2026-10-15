CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
MODULE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
ROOT := $(abspath $(MODULE_DIR)../..)
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
PATCH ?= 0
TARGET := $(MODULE_DIR)bin/hotload_probe.so
TEST_TARGET := $(MODULE_DIR)bin/test_hotload_probe
VERSION_FLAG := -DHOTLOAD_PROBE_PATCH=$(PATCH)

.PHONY: all clean test install
all: $(TARGET)

$(TARGET): $(MODULE_DIR)src/hotload_probe.c
	mkdir -p "$(MODULE_DIR)bin"
	$(CC) $(CFLAGS) $(VERSION_FLAG) -I"$(ROOT)/include" -shared "$<" -o "$@"

$(TEST_TARGET): $(MODULE_DIR)tests/test_hotload_probe.c $(MODULE_DIR)src/hotload_probe.c
	mkdir -p "$(MODULE_DIR)bin"
	$(CC) $(CFLAGS) $(VERSION_FLAG) -I"$(ROOT)/include" $(MODULE_DIR)tests/test_hotload_probe.c $(MODULE_DIR)src/hotload_probe.c -o "$@"

test: $(TARGET) $(TEST_TARGET)
	"$(TEST_TARGET)"

install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/hotload_probe" "$(PREFIX)/modules/hotload_probe/bin"
	install -m 0640 "$(MODULE_DIR)module.conf" "$(PREFIX)/modules/hotload_probe/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/hotload_probe/bin/hotload_probe.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/hotload_probe"

clean:
	rm -rf "$(MODULE_DIR)bin"
