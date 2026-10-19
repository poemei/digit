CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/observation
TARGET := bin/observation.so
TEST_TARGET := bin/test_observation
.PHONY: all clean install test
all: $(TARGET)
$(TARGET): observation.c observation.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared observation.c -o $@
$(TEST_TARGET): test/test_observation.c ../../core/stnlabz_abi/module_loader.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test/test_observation.c -o $@ -ldl
test: $(TARGET) $(TEST_TARGET)
	./$(TEST_TARGET) ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/observation.so"
clean:
	rm -rf bin
