# [AI:GPT-6 | 2026-10-22] Slack ABI 1.5 module build
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
LDLIBS += -lcurl -ljson-c -pthread
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/slack
TARGET := bin/slack.so
.PHONY: all clean install test
all: $(TARGET)
$(TARGET): slack.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared slack.c -o $@ $(LDLIBS)
test: $(TARGET)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test_slack.c -o bin/test_slack -ldl
	./bin/test_slack ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/slack.so"
clean:
	rm -rf bin
