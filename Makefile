# STN-LABZ Digit executable (Linux)
# The shared STN-LABZ ABI is maintained outside this repository.
#
# Default layout:
#   ../ABI/includes/*.h
#   ../ABI/src/*.c
# Follow Digit's Core ABI source selection; exclude standalone ABI inventory/catalog.
# Override ABI_DIR / ABI_INCLUDE_DIR / ABI_SRC_DIR for a different layout.

CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?= -ldl -lcrypto

ABI_DIR ?= ../ABI
ABI_INCLUDE_DIR ?= $(ABI_DIR)/includes
ABI_SRC_DIR ?= $(ABI_DIR)/src

CPPFLAGS += -Iinclude -I$(ABI_INCLUDE_DIR) -Iinclude/stnlabz_abi

BUILD_DIR ?= build
TARGET ?= bin/digit

PREFIX ?= /opt/digit
BINDIR ?= $(PREFIX)/bin
LOGDIR ?= $(PREFIX)/logs
MODULEDIR ?= $(PREFIX)/modules
CONFIGDIR ?= $(PREFIX)/config
STATEDIR ?= $(PREFIX)/state

CORE_SRCS := $(sort $(wildcard src/*.c))
ABI_SRCS := $(addprefix $(ABI_SRC_DIR)/,abi.c module.c module_registry.c loader_linux.c module_discovery_linux.c)
CORE_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SRCS))
ABI_OBJS := $(patsubst $(ABI_SRC_DIR)/%.c,$(BUILD_DIR)/abi/%.o,$(ABI_SRCS))
OBJS := $(CORE_OBJS) $(ABI_OBJS)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean install uninstall test test-contracts test-behavior test-hotload test-concurrency test-reentrant test-inflight
all: $(TARGET)

.PHONY: check-abi
check-abi:
	@test -f "$(ABI_SRC_DIR)/abi.c" || { \
		echo "ERROR: Shared ABI C sources not found in $(ABI_SRC_DIR)" >&2; \
		echo "Set ABI_DIR or ABI_SRC_DIR to the actual external ABI location." >&2; \
		exit 1; \
	}

$(TARGET): $(OBJS) | check-abi
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/abi/%.o: $(ABI_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Unit-level C behavior tests are distinct from Core integration qualification.
$(BUILD_DIR)/test_core_behavior: tests/test_core_behavior.c src/agent_root.c src/identity_digit.c src/core_policy.c src/logging.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

test-contracts:
	python3 -m unittest discover -s tests -p 'test_*.py' -v

test-behavior: $(BUILD_DIR)/test_core_behavior
	./$(BUILD_DIR)/test_core_behavior

# Test fixtures are compiled as separate real shared objects. Test staging,
# module installation and log files remain under a disposable private /tmp tree.
HOTLOAD_FIXTURES := $(addprefix $(BUILD_DIR)/fixtures/,old.so badqual.so badstart.so good.so)
HOTLOAD_TEST_SRCS := src/core_hotload.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c src/module_host.c src/core_diagnostics.c
HOTLOAD_ABI_SRCS := $(addprefix $(ABI_SRC_DIR)/,abi.c module.c module_registry.c loader_linux.c)

$(BUILD_DIR)/fixtures/old.so: tests/fixtures/hotload_fixture.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DFIXTURE_VERSION=0 -o $@ $<

$(BUILD_DIR)/fixtures/badqual.so: tests/fixtures/hotload_fixture.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DFIXTURE_VERSION=1 -DFIXTURE_BAD_QUAL=1 -o $@ $<

$(BUILD_DIR)/fixtures/badstart.so: tests/fixtures/hotload_fixture.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DFIXTURE_VERSION=1 -DFIXTURE_BAD_START=1 -o $@ $<

$(BUILD_DIR)/fixtures/good.so: tests/fixtures/hotload_fixture.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DFIXTURE_VERSION=2 -o $@ $<

$(BUILD_DIR)/test_core_hotload: tests/test_core_hotload.c $(HOTLOAD_TEST_SRCS) $(HOTLOAD_ABI_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDIGIT_HOTLOAD_TEST_STAGE=1 -o $@ $^ $(LDLIBS)

test-hotload: $(BUILD_DIR)/test_core_hotload $(HOTLOAD_FIXTURES)
	./$(BUILD_DIR)/test_core_hotload $(HOTLOAD_FIXTURES)

$(BUILD_DIR)/test_core_concurrency: tests/test_core_concurrency.c src/module_host.c src/core_diagnostics.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -o $@ $^

test-concurrency: $(BUILD_DIR)/test_core_concurrency
	./$(BUILD_DIR)/test_core_concurrency

$(BUILD_DIR)/test_core_self_unregister: tests/test_core_self_unregister.c src/module_host.c src/core_diagnostics.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^

test-reentrant: $(BUILD_DIR)/test_core_self_unregister
	./$(BUILD_DIR)/test_core_self_unregister

$(BUILD_DIR)/fixtures/blocking.so: tests/fixtures/hotload_fixture.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DFIXTURE_VERSION=0 -DFIXTURE_REGISTER_SERVICE=1 -o $@ $<

$(BUILD_DIR)/test_core_hotload_inflight: tests/test_core_hotload_inflight.c $(HOTLOAD_TEST_SRCS) $(HOTLOAD_ABI_SRCS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DDIGIT_HOTLOAD_TEST_STAGE=1 -o $@ $^ $(LDLIBS)

test-inflight: $(BUILD_DIR)/test_core_hotload_inflight $(BUILD_DIR)/fixtures/blocking.so $(BUILD_DIR)/fixtures/good.so
	./$(BUILD_DIR)/test_core_hotload_inflight $(BUILD_DIR)/fixtures/blocking.so $(BUILD_DIR)/fixtures/good.so

test: $(TARGET) test-contracts test-behavior test-hotload test-concurrency test-reentrant test-inflight
	@echo "Core source, C behavior, hotload integration, and threaded handler-withdrawal and self-unregister checks complete; live module concurrency qualification remains separate."

install: $(TARGET)
	install -d $(BINDIR) $(LOGDIR) $(MODULEDIR) $(CONFIGDIR) $(STATEDIR)
	install -m 0755 $(TARGET) $(BINDIR)/digit

uninstall:
	rm -f $(BINDIR)/digit

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
