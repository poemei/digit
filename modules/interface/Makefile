CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/interface.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/interface.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared src/interface.c -o $@ -lssl -lcrypto -lpthread
bin/test_interface: tests/test_interface.c src/interface.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include tests/test_interface.c src/interface.c -o $@ -lssl -lcrypto -lpthread
test: $(TARGET) bin/test_interface
	@./bin/test_interface
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/interface" "$(PREFIX)/modules/interface/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/interface/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/interface/bin/interface.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/interface"
clean:
	rm -rf bin
