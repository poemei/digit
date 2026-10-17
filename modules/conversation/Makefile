CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/conversation.so
PREFIX ?= /opt/digit
.PHONY: all test install clean
all: $(TARGET)
$(TARGET): src/conversation.c include/conversation.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/conversation.c -o $@
bin/test_conversation: tests/test_conversation.c src/conversation.c include/conversation.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_conversation.c src/conversation.c -o $@
test: $(TARGET) bin/test_conversation
	./bin/test_conversation
install: $(TARGET)
	@install -d -m 0750 "$(PREFIX)/modules/conversation/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/conversation/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/conversation/bin/conversation.so"
clean:
	rm -rf bin
