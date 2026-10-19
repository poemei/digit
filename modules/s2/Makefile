# Independent S2 build; no Core or other module compilation.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I. -I../../core/stnlabz_abi
TARGET := bin/s2.so
TEST_TARGET := bin/test_s2
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/s2
.PHONY: all test install clean
all: $(TARGET)
$(TARGET): s2.c s2.h s2_observations.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared s2.c -o $@ -lcurl -ljson-c -pthread
$(TEST_TARGET): test/test_s2.c s2.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_s2.c -o $@ -ldl
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/s2.so"
clean:
	rm -rf bin
