# [AI:GPT-6 | 2026-10-18 23:50:00 UTC]
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Icore -Icore/stnlabz_abi -Imodels -Iviews -Icontrollers
LDLIBS += -lssl -lcrypto -ldl
TARGET := bin/digit
SOURCES := core/agent_root.c core/identity_digit.c models/report.c views/irc_render.c controllers/irc.c controllers/main.c
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
	$(CC) $(CPPFLAGS) $(CFLAGS) core/agent_root.c core/identity_digit.c models/report.c views/irc_render.c test/test_core.c -o bin/test_core
# [AI:GPT-6 | 2026-10-19 00:13:40 UTC] Test footer reflects actual execution, not qualification.
	@status=0; ./bin/test_core || status=$?; \
	 if [ "$status" -eq 0 ]; then \
	   printf '\nCore checks: PASS (9 assertions)\n'; \
	   printf 'Agent qualification: PENDING (minimum 20 tests required)\n'; \
	   printf 'OVERALL TEST COMPLETED: PASS (current suite only)\n'; \
	 else \
	   printf '\nCore checks: FAIL (exit code %s)\n' "$status"; \
	   printf 'Agent qualification: NOT MET\n'; \
	   printf 'OVERALL TEST COMPLETED: FAIL\n'; \
	 fi; exit "$status"
# [End AI:GPT-6]
clean:
	rm -rf bin