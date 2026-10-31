CC ?= cc
ROOT ?= ../..
CPPFLAGS ?=
CPPFLAGS += -Iinclude -I$(ROOT)/include -I../research/include
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
MODULE := ask
BUILD := build
TARGET := $(BUILD)/ask.so
TEST := $(BUILD)/test_ask
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/ask
MODULE_USER ?= digit
MODULE_GROUP ?= digit

.PHONY: all test install clean
all: $(TARGET)
$(TARGET): src/ask_module.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LDFLAGS) -o $@ src/ask_module.c
$(TEST): tests/test_ask.c src/ask_module.c
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_ask.c
test: $(TEST)
	./$(TEST)
install: $(TARGET) test
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "ERROR: missing module user"; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "ERROR: missing module group"; exit 1; }
	@install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.ask.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage" || exit 1; \
	  mv -f "$$stage" "$(MODULEDIR)/ask.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/ask.so"; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/ask.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
	@install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0644 module.conf "$(MODULEDIR)/module.conf"
	@cmp -s module.conf "$(MODULEDIR)/module.conf"
clean:
	rm -rf $(BUILD)
