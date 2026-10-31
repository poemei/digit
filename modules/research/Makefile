CC ?= cc
ROOT ?= ../..
CPPFLAGS ?=
CPPFLAGS += -Iinclude -I$(ROOT)/include
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?= -lcurl -ljson-c -lpthread

MODULE := research
BUILD := build
TARGET := $(BUILD)/$(MODULE).so
TEST := $(BUILD)/test_research
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/$(MODULE)
MODULE_USER ?= digit
MODULE_GROUP ?= digit

.PHONY: all test install clean
all: $(TARGET)

$(TARGET): src/research.c src/research_engine.c src/research_engine.h include/research.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $(LDFLAGS) -o $@ src/research.c src/research_engine.c $(LDLIBS)

$(TEST): tests/test_research.c src/research.c src/research_engine.c src/research_engine.h include/research.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_research.c src/research_engine.c $(LDLIBS)

test: $(TEST)
	./$(TEST)

install: $(TARGET) test
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "missing module owner $(MODULE_USER)"; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "missing module group $(MODULE_GROUP)"; exit 1; }
	@install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.research.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage"; \
	  mv -f "$$stage" "$(MODULEDIR)/research.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/research.so"; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/research.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
	@install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0644 module.conf "$(MODULEDIR)/module.conf"
	@cmp -s module.conf "$(MODULEDIR)/module.conf"
	@echo "Research module installed and verified: $(MODULEDIR)"

clean:
	rm -rf $(BUILD)
