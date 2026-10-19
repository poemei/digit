# Independent SQLite3 module: root Core Makefile must not build or install modules.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/sqlite3.so
TEST_TARGET := bin/test_sqlite3
PREFIX ?= /opt/digit
DESTDIR ?=
MODULE_DIR := $(PREFIX)/modules/sqlite3

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): sqlite3.c sqlite3_diagnostic.h corpus_search.h ../dsd/dsd.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared sqlite3.c -o $@ -lsqlite3
$(TEST_TARGET): test/test_sqlite3.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_sqlite3.c -o $@ -ldl
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/sqlite3.so"
clean:
	rm -rf bin
