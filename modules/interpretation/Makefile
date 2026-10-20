CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/interpretation.so
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/interpretation

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): interpretation.c interpretation.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared interpretation.c -o $@

test: $(TARGET)
	@echo "Build only. ABI qualification requires Digit Core execution."

install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/interpretation.so"

clean:
	rm -rf bin
