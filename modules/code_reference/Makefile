CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/code_reference.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/code_reference.c include/code_reference.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/code_reference.c -o $@
bin/test_code_reference: tests/test_code_reference.c src/code_reference.c include/code_reference.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_code_reference.c src/code_reference.c -o $@
test: $(TARGET) bin/test_code_reference
	@echo '-----------'
	@echo 'CODE REFERENCE MODULE TESTS'
	@./bin/test_code_reference
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/code_reference" "$(PREFIX)/modules/code_reference/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/code_reference/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/code_reference/bin/code_reference.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/code_reference"
clean:
	rm -rf bin
