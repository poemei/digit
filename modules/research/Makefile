CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/research.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/research.c $(ROOT)/include/research_contract.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared src/research.c -o $@ -lcurl -lpthread
bin/test_research: tests/test_research.c src/research.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include tests/test_research.c src/research.c -o $@ -lcurl -lpthread
test: $(TARGET) bin/test_research
	./bin/test_research
install: $(TARGET)
	install -d -m 0750 "$(PREFIX)/modules/research" "$(PREFIX)/modules/research/bin"
	install -m 0640 module.conf "$(PREFIX)/modules/research/module.conf"
	install -m 0750 "$(TARGET)" "$(PREFIX)/modules/research/bin/research.so"
	chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/research"
clean:
	rm -rf bin
