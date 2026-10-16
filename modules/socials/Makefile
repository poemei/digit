CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/socials.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/socials.c include/socials.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/socials.c -o $@
bin/test_socials: tests/test_socials.c src/socials.c include/socials.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_socials.c src/socials.c -o $@
test: $(TARGET) bin/test_socials
	./bin/test_socials
install: $(TARGET)
	install -d -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(PREFIX)/modules/socials" "$(PREFIX)/modules/socials/bin"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0640 module.conf "$(PREFIX)/modules/socials/module.conf"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(TARGET)" "$(PREFIX)/modules/socials/bin/socials.so"
clean:
	rm -rf bin
