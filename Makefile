# [AI:GPT-6 | 2026-10-18 23:50:00 UTC]
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Icore -Icore/stnlabz_abi -Imodels -IViews -Icontrollers
LDLIBS += -lssl -lcrypto -ldl
TARGET := bin/digit
SOURCES := core/agent_root.c core/identity_digit.c models/report.c Views/irc_render.c controllers/irc.c controllers/main.c
CORE_SOURCES := $(filter-out core/main.c,$(wildcard core/*.c)) $(wildcard core/stnlabz_abi/*.c)
CORE_TARGET := bin/digit-core
.PHONY: all clean test
all: $(TARGET) $(CORE_TARGET)
$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)
$(CORE_TARGET): $(CORE_SOURCES) core/main.c
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_SOURCES) core/main.c -o $(CORE_TARGET) $(LDLIBS)
test: $(TARGET) $(CORE_TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) core/agent_root.c core/identity_digit.c models/report.c Views/irc_render.c test/test_core.c -o bin/test_core
	./bin/test_core
clean:
	rm -rf bin