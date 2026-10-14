CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/slack.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/slack.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared $< -o $@ -lcurl -ljson-c -lpthread
bin/test_slack: tests/test_slack.c src/slack.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include tests/test_slack.c src/slack.c -o $@ -lcurl -ljson-c -lpthread
test: $(TARGET) bin/test_slack
	./bin/test_slack
install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/slack" "$(PREFIX)/modules/slack/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/slack/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/slack/bin/slack.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/slack"
clean:
	rm -rf bin
