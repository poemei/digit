CC ?= cc
ROOT ?= ../..
CPPFLAGS += -Iinclude -I$(ROOT)/include $(shell pkg-config --cflags libxml-2.0)
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += -lcurl $(shell pkg-config --libs libxml-2.0)
BUILD := build
TARGET := $(BUILD)/research.so
TEST := $(BUILD)/test_research
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/research
MODULE_USER ?= digit
MODULE_GROUP ?= digit
.PHONY: all test clean install
all: $(TARGET)
$(BUILD):
	mkdir -p $@
$(TARGET): src/research.c include/research.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $< $(LDLIBS)
$(TEST): tests/test_research.c src/research.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)
test: $(TEST)
	./$(TEST)
install: $(TARGET) test
	@id -u "$(MODULE_USER)" >/dev/null && getent group "$(MODULE_GROUP)" >/dev/null
	install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	@set -eu; stage=$$(mktemp "$(MODULEDIR)/.research.so.XXXXXX"); \
	 trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	 install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	 cmp -s "$(TARGET)" "$$stage"; mv -f "$$stage" "$(MODULEDIR)/research.so"; \
	 trap - EXIT HUP INT TERM; cmp -s "$(TARGET)" "$(MODULEDIR)/research.so"; \
	 test "$$(stat -c %U:%G "$(MODULEDIR)/research.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
clean:
	rm -rf $(BUILD)
