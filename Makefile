# [AI:GPT-6 | 2026-10-18 23:50:00 UTC]
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Icore -Icore/stnlabz_abi -Imodels -Iviews -Icontrollers
LDLIBS += -lssl -lcrypto -ldl
TARGET := bin/digit
SOURCES = $(CORE_SOURCES) core/main.c
CORE_SOURCES := $(filter-out core/main.c,$(wildcard core/*.c)) $(wildcard core/stnlabz_abi/*.c)
CORE_TARGET := bin/digit-core
PREFIX ?= /opt/digit
DESTDIR ?=
.PHONY: all clean test install install-systemd
all: $(TARGET) $(CORE_TARGET)
$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)
$(CORE_TARGET): $(CORE_SOURCES) core/main.c
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_SOURCES) core/main.c -o $(CORE_TARGET) $(LDLIBS)
install: all
	install -d -m 0700 "$(DESTDIR)$(PREFIX)/logs"
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/digit"
	install -m 0755 $(CORE_TARGET) "$(DESTDIR)$(PREFIX)/bin/digit-core"
# [End AI:GPT-6]

# [AI:GPT-6 | 2026-10-18] Never clobber an installed systemd service.
install-systemd:
	@if [ -e /etc/systemd/system/digit.service ] || \
	    [ -e /lib/systemd/system/digit.service ] || \
	    [ -e /usr/lib/systemd/system/digit.service ]; then \
	    echo "digit.service already exists; refusing to overwrite it"; exit 1; \
	fi
	install -d /etc/systemd/system
	install -m 0644 systemd/digit.service /etc/systemd/system/digit.service
	@echo "Unit installed, not enabled or started. Review then run systemctl daemon-reload."
# [End AI:GPT-6]

test: $(TARGET) $(CORE_TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) core/agent_root.c core/identity_digit.c models/report.c views/irc_render.c test/test_core.c -o bin/test_core
# [AI:GPT-6] Run shell reporter without Make dollar escaping.
	$(CC) $(CPPFLAGS) $(CFLAGS) core/transports.c test/test_transports.c -o bin/test_transports
	$(CC) $(CPPFLAGS) $(CFLAGS) core/logging.c test/test_logging.c -o bin/test_logging
	@sh test/run_core_tests.sh
# [End AI:GPT-6]
clean:
	rm -rf bin