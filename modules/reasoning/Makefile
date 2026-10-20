CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/reasoning.so
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/reasoning
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): reasoning.c reasoning.h ../interpretation/interpretation.h ../sqlite3/corpus_search.h ../../core/stnlabz_abi/module.h ../../core/stnlabz_abi/dsd.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared reasoning.c -o $@
test: $(TARGET)
	@echo "Build only: Core runs ABI module qualification."
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/reasoning.so"
clean:
	rm -rf bin
