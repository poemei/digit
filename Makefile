CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -ICore -ICore/stnlabz_abi -IModel -IView -IController
LDLIBS += -lssl -lcrypto -ldl
TARGET := bin/digit
SOURCES := Core/agent_root.c Core/identity_digit.c Model/report.c View/irc_render.c Controller/irc.c Controller/main.c
CORE_SOURCES := $(filter-out Core/main.c,$(wildcard Core/*.c)) $(wildcard Core/stnlabz_abi/*.c)
CORE_TARGET := bin/digit-core
.PHONY: all clean test
all: $(TARGET) $(CORE_TARGET)
$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)
$(CORE_TARGET): $(CORE_SOURCES) Core/main.c
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_SOURCES) Core/main.c -o $(CORE_TARGET) $(LDLIBS)
test: $(TARGET) $(CORE_TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) Core/agent_root.c Core/identity_digit.c Model/report.c View/irc_render.c tests/test_core.c -o bin/test_core
	./bin/test_core
clean:
	rm -rf bin
