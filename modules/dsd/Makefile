CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I. -I../../core/stnlabz_abi
TARGET := bin/dsd.so
TEST_TARGET := bin/test_dsd
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/dsd
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): dsd.c dsd.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared dsd.c -o $@ -pthread
$(TEST_TARGET): test/test_dsd.c dsd.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_dsd.c -o $@ -ldl -pthread
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/dsd.so"
clean:
	rm -rf bin
