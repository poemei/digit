CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/observation
TARGET := bin/observation.so
.PHONY: all clean install
all: $(TARGET)
$(TARGET): observation.c observation.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared observation.c -o $@
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/observation.so"
clean:
	rm -rf bin
