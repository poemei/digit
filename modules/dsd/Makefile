CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/dsd.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean install test
all: $(TARGET)
$(TARGET): src/dsd.c include/dsd.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -I$(ROOT)/include/stnlabz_abi -shared src/dsd.c -o $@
bin/test_dsd: tests/test_dsd.c src/dsd.c include/dsd.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -I$(ROOT)/include/stnlabz_abi tests/test_dsd.c src/dsd.c -o $@
test: $(TARGET) bin/test_dsd
	@echo '-----------'
	@echo 'DSD MODULE TESTS'
	@DIGIT_SOURCE_ROOT="$(abspath $(ROOT))" ./bin/test_dsd
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first: $(PREFIX)/modules missing" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/dsd" "$(PREFIX)/modules/dsd/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/dsd/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/dsd/bin/dsd.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/dsd"
clean:
	rm -rf bin
