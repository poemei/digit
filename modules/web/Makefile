CC ?= cc
ROOT ?= ../..
CPPFLAGS ?=
CPPFLAGS += -Iinclude -I$(ROOT)/include
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?= -lcurl -ljson-c -pthread
BUILD := build
TARGET := $(BUILD)/web.so
TEST := $(BUILD)/test_web
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/web
MODULE_USER ?= digit
MODULE_GROUP ?= digit

.PHONY: all test install clean
all: $(TARGET)
$(BUILD):
	mkdir -p $@
$(TARGET): src/web_module.c include/web_transport.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LDFLAGS) -o $@ src/web_module.c $(LDLIBS)
$(TEST): tests/test_web.c src/web_module.c include/web_transport.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_web.c $(LDLIBS)
test: $(TEST)
	./$(TEST)
install: $(TARGET) test module.conf
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "Missing module user"; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "Missing module group"; exit 1; }
	@install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.web.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage"; \
	  mv -f "$$stage" "$(MODULEDIR)/web.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/web.so"; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/web.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
	@install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0644 module.conf "$(MODULEDIR)/module.conf"
	@cmp -s module.conf "$(MODULEDIR)/module.conf"
clean:
	rm -rf $(BUILD)
