CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
ROOT ?= ../..
BUILD_DIR ?= build
PREFIX ?= /opt/digit
MODULEDIR ?= $(PREFIX)/modules/research
CPPFLAGS += -Iinclude -I$(ROOT)/include
SOURCES := src/research_module.c src/research_model.c src/research_qualification.c
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
.PHONY: all test install clean
all: $(BUILD_DIR)/research.so
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@
$(BUILD_DIR)/research.so: $(OBJS)
	$(CC) -shared $(LDFLAGS) -o $@ $^
$(BUILD_DIR)/test_research: tests/test_research.c src/research_model.c src/research_qualification.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^
test: $(BUILD_DIR)/test_research
	./$(BUILD_DIR)/test_research
install: all
	install -d $(MODULEDIR)
	install -m 0644 module.conf $(MODULEDIR)/module.conf
	install -m 0755 $(BUILD_DIR)/research.so $(MODULEDIR)/research.so
clean:
	rm -rf $(BUILD_DIR)
-include $(OBJS:.o=.d)
