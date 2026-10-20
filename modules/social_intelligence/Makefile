CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/social_intelligence.so
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/social_intelligence
.PHONY: all test install clean
all: $(TARGET)
$(TARGET): social_intelligence.c social_intelligence.h ../socials/socials.h ../interpretation/interpretation.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared social_intelligence.c -o $@
test: $(TARGET)
	@echo "Qualification executes through Digit Core module descriptor."
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/social_intelligence.so"
clean:
	rm -rf bin
