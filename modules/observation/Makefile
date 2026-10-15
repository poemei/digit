CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/observation.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all clean test install
all: $(TARGET)
$(TARGET): src/observation.c include/observation.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/observation.c -o $@ -ljson-c -lpthread
bin/test_observation: tests/test_observation.c src/observation.c
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include tests/test_observation.c src/observation.c -o $@ -ljson-c -lpthread
test: $(TARGET) bin/test_observation
	./bin/test_observation
install: $(TARGET)
	install -d -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(PREFIX)/observation"
	install -d -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(PREFIX)/modules/observation" "$(PREFIX)/modules/observation/bin"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0640 module.conf "$(PREFIX)/modules/observation/module.conf"
	install -o "$(OWNER)" -g "$(GROUP)" -m 0750 "$(TARGET)" "$(PREFIX)/modules/observation/bin/observation.so"
clean:
	rm -rf bin
