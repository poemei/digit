CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
TARGET := bin/gru_bridge.so
PROBE := bin/test_gru_bridge
LEARN_PROBE := bin/test_gru_learn
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/gru_bridge
.PHONY: all test integration-test learn-probe install clean
all: $(TARGET)
$(TARGET): gru_bridge.c gru_bridge.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared gru_bridge.c -o $@ -lcurl -ljson-c
$(PROBE): test_bridge.c gru_bridge.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test_bridge.c -o $@ -ldl
$(LEARN_PROBE): test_learn.c gru_bridge.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test_learn.c -o $@ -ldl
test: $(TARGET)
	@echo "Qualification runs through Digit module loader; no network used during qualification."
integration-test: $(TARGET) $(PROBE)
	./$(PROBE) ./$(TARGET) "The engineer "
learn-probe: $(TARGET) $(LEARN_PROBE)
	@echo "Explicit learning probe built; NOT executed. Requires human-reviewed input and --approved."
install: $(TARGET)
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(MODULE_DIR)/bin/gru_bridge.so"
clean:
	rm -rf bin
