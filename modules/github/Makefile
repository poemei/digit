CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?= -lcurl -ljson-c
ROOT ?= ../..
CPPFLAGS += -Iinclude -I$(ROOT)/include
BUILD := build
TARGET := $(BUILD)/github.so
TEST := $(BUILD)/test_github
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/github
MODULE_USER ?= digit
MODULE_GROUP ?= digit

.PHONY: all test clean install
all: $(TARGET)

$(BUILD):
	mkdir -p $@

$(TARGET): src/github.c include/github_module.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LDFLAGS) -o $@ src/github.c $(LDLIBS)

$(TEST): tests/test_github.c tests/approved.conf src/github.c include/github_module.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DGITHUB_ALLOWLIST_PATH='"$(CURDIR)/tests/approved.conf"' \
	  $(LDFLAGS) -o $@ tests/test_github.c src/github.c $(LDLIBS)

test: $(TEST)
	./$(TEST)

install: $(TARGET) test module.conf
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "ERROR: missing user $(MODULE_USER)" >&2; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "ERROR: missing group $(MODULE_GROUP)" >&2; exit 1; }
	@install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0750 "$(MODULEDIR)"
	@install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0644 module.conf "$(MODULEDIR)/module.conf"
	@test -f "$(MODULEDIR)/repos.conf" || { echo "ERROR: operator-approved repos.conf required before install" >&2; exit 1; }
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.github.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage"; \
	  mv -f "$$stage" "$(MODULEDIR)/github.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/github.so"; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/github.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
	@cmp -s module.conf "$(MODULEDIR)/module.conf"

clean:
	rm -rf $(BUILD)
