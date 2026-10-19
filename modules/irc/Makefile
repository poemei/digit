# [AI:GPT-6 | 2026-10-18]
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
LDLIBS += -lssl -lcrypto -pthread
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/irc
TARGET := bin/irc.so
TEST_TARGET := bin/test_irc

.PHONY: all clean test install
all: $(TARGET)

$(TARGET): irc.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared irc.c -o $@ $(LDLIBS)

$(TEST_TARGET): test/test_irc.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_irc.c -o $@ -ldl

test: $(TARGET) $(TEST_TARGET)
	@./$(TEST_TARGET) ./$(TARGET)
	@printf 'IRC offline qualification checks completed; activation still requires Core authorization.\n'

install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/irc.so"

clean:
	rm -rf bin
# [End AI:GPT-6]
