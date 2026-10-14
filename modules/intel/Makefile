CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/intel.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/intel.c includes/intel.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iincludes -I$(ROOT)/include -shared src/intel.c -o $@ -lcurl -ljson-c -lpthread
bin/test_intel: tests/test_intel.c src/intel.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Iincludes -I$(ROOT)/include tests/test_intel.c src/intel.c -o $@ -lcurl -ljson-c -lpthread
test: $(TARGET) bin/test_intel
	./bin/test_intel
install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/intel" "$(PREFIX)/modules/intel/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/intel/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/intel/bin/intel.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/intel"
clean:
	rm -rf bin
