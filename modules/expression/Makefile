CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/expression.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all test install clean
all: $(TARGET)
$(TARGET): src/expression.c ../corpus/src/corpus_store.c ../corpus/include/corpus_store.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -I../corpus/include -shared src/expression.c ../corpus/src/corpus_store.c -o $@
bin/test_expression: tests/test_expression.c src/expression.c ../corpus/src/corpus_store.c ../corpus/include/corpus_store.h
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -I../corpus/include tests/test_expression.c ../corpus/src/corpus_store.c -o $@

test: $(TARGET) bin/test_expression
	@./bin/test_expression

install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/expression" "$(PREFIX)/modules/expression/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/expression/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/expression/bin/expression.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/expression"
clean:
	rm -rf bin
