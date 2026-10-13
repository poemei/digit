CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/gfm.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/gfm.c include/gfm.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/gfm.c -o $@
bin/test_gfm: tests/test_gfm.c src/gfm.c include/gfm.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_gfm.c src/gfm.c -o $@
test: $(TARGET) bin/test_gfm
	@./bin/test_gfm
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/gfm" "$(PREFIX)/modules/gfm/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/gfm/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/gfm/bin/gfm.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/gfm"
clean:
	rm -rf bin
