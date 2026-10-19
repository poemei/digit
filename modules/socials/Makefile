# Independently built Socials module. Core Makefile never builds modules.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I. -I../../core/stnlabz_abi
TARGET := bin/socials.so
TEST_TARGET := bin/test_socials
PREFIX ?= /opt/digit
DESTDIR ?=
MODULE_DIR := $(PREFIX)/modules/socials

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): socials.c socials.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared socials.c -o $@
$(TEST_TARGET): test/test_socials.c socials.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_socials.c -o $@ -ldl
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/socials.so"
clean:
	rm -rf bin
