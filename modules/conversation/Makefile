CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/conversation.so
TEST_TARGET := bin/test_conversation
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/conversation

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): conversation.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared conversation.c -o $@
$(TEST_TARGET): test/test_conversation.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_conversation.c -o $@ -ldl
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/conversation.so"
clean:
	rm -rf bin
