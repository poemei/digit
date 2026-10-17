CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/ask.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/ask.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -I../observation/include -I../reasoning/include -I../recognition/include -I../socials/include -shared src/ask.c -o $@
bin/test_ask: tests/test_ask.c src/ask.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -I../observation/include -I../reasoning/include -I../recognition/include -I../socials/include tests/test_ask.c src/ask.c -o $@
test: $(TARGET) bin/test_ask
	@./bin/test_ask
install: $(TARGET)
	@install -d -m 0750 "$(PREFIX)/modules/ask" "$(PREFIX)/modules/ask/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/ask/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/ask/bin/ask.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/ask"
	@printf "ASK INSTALLED: $(PREFIX)/modules/ask/bin/ask.so (module restart/activation required)\\n"
clean:
	rm -rf bin
