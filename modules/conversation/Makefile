CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/conversation.so
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/conversation

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): conversation.c response_view.c response_view.h ../interpretation/interpretation.h ../socials/socials.h ../observation/observation.h ../sqlite3/context.h ../sqlite3/corpus_search.h ../reasoning/reasoning.h ../../core/stnlabz_abi/module.h ../../core/stnlabz_abi/identity_read.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared conversation.c response_view.c -o $@
test: $(TARGET)
	@echo "Build only; qualification executes when Digit Core verifies Conversation v1.0.10."
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/conversation.so"
clean:
	rm -rf bin
