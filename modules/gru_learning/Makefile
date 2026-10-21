CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/gru_learning.so
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/gru_learning
.PHONY: all test install clean
all: $(TARGET)
$(TARGET): gru_learning.c gru_learning.h ../observation/observation.h ../sqlite3/corpus_search.h ../sqlite3/context.h ../sqlite3/gru_feed.h ../gru_bridge/gru_bridge.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared gru_learning.c -o $@ -pthread
test: $(TARGET)
	@echo "Offline qualification is performed by the Digit module loader."
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/gru_learning.so"
clean:
	rm -rf bin
