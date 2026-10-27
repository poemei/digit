CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
ROOT ?= ../..
CPPFLAGS += -Iinclude -I$(ROOT)/include
LDLIBS ?= -lcurl -ljson-c -lpthread
MODULE := s2
BUILD := build
TARGET := $(BUILD)/s2.so
TEST := $(BUILD)/test_s2
PREFIX ?= /opt/digit
MODULEDIR := $(PREFIX)/modules/s2
MODULE_USER ?= digit
MODULE_GROUP ?= digit
SOURCES := src/s2_model.c src/s2_module.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES))
.PHONY: all test install clean
all: $(TARGET)
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@
$(TARGET): $(OBJS)
	$(CC) -shared $(LDFLAGS) -o $@ $^ $(LDLIBS)
$(TEST): tests/test_s2.c src/s2_model.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ljson-c
test: $(TEST)
	./$(TEST)
install: $(TARGET)
	@id -u "$(MODULE_USER)" >/dev/null 2>&1 || { echo "missing module owner"; exit 1; }
	@getent group "$(MODULE_GROUP)" >/dev/null 2>&1 || { echo "missing module group"; exit 1; }
	install -d -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(MODULEDIR)"
	@set -eu; \
	  stage=$$(mktemp "$(MODULEDIR)/.s2.so.XXXXXX"); \
	  trap 'rm -f "$$stage"' EXIT HUP INT TERM; \
	  install -o "$(MODULE_USER)" -g "$(MODULE_GROUP)" -m 0755 "$(TARGET)" "$$stage"; \
	  cmp -s "$(TARGET)" "$$stage"; \
	  mv -f "$$stage" "$(MODULEDIR)/s2.so"; \
	  trap - EXIT HUP INT TERM; \
	  cmp -s "$(TARGET)" "$(MODULEDIR)/s2.so"; \
	  test "$$(stat -c %U:%G "$(MODULEDIR)/s2.so")" = "$(MODULE_USER):$(MODULE_GROUP)"
clean:
	rm -rf $(BUILD)
-include $(OBJS:.o=.d)
