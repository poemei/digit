CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?= -lcurl

ROOT ?= ../..
CPPFLAGS += -Iinclude -I$(ROOT)/include
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LDFLAGS) -o $@ src/research.c $(LDLIBS)

$(TEST): tests/test_research.c src/research.c include/research.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_research.c src/research.c $(LDLIBS)

test: $(TEST)
	./$(TEST)

install: $(TARGET) test module.conf
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "ERROR: missing install user $(MODULE_USER)" >&2; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "ERROR: missing install group $(MODULE_GROUP)" >&2; exit 1; }
	install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0644 module.conf "$(MODULEDIR)/module.conf"
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.research.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage" || { echo "ERROR: staged Research module differs from build" >&2; exit 1; }; \
	  mv -f "$$stage" "$(MODULEDIR)/research.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/research.so" || { echo "ERROR: installed Research module differs from build" >&2; exit 1; }; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/research.so")" = "$(MODULE_USER):$(MODULE_GROUP)" || { echo "ERROR: Research module ownership incorrect" >&2; exit 1; }; \
	  echo "Installed verified research.so as $(MODULE_USER):$(MODULE_GROUP)"
	@cmp -s module.conf "$(MODULEDIR)/module.conf" || { echo "ERROR: Research manifest differs from source" >&2; exit 1; }

clean:
	rm -rf $(BUILD)
