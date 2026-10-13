CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/corpus.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all install clean
all: $(TARGET)
$(TARGET): src/corpus.c src/corpus_store.c include/corpus_store.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -shared src/corpus.c src/corpus_store.c -o $@
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/corpus" "$(PREFIX)/modules/corpus/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/corpus/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/corpus/bin/corpus.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/corpus"
clean:
	rm -rf bin
