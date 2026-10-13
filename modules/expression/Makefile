CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/expression.so
PREFIX ?= /opt/digit
OWNER ?= digit
GROUP ?= digit
.PHONY: all install clean
all: $(TARGET)
$(TARGET): src/expression.c
	mkdir -p bin
	$(CC) $(CFLAGS) -I$(ROOT)/include -shared src/expression.c -o $@
install: $(TARGET)
	@test -d "$(PREFIX)/modules" || { echo "Install Digit first" >&2; exit 2; }
	@install -d -m 0750 "$(PREFIX)/modules/expression" "$(PREFIX)/modules/expression/bin"
	@install -m 0640 module.conf "$(PREFIX)/modules/expression/module.conf"
	@install -m 0750 "$(TARGET)" "$(PREFIX)/modules/expression/bin/expression.so"
	@chown -R "$(OWNER):$(GROUP)" "$(PREFIX)/modules/expression"
clean:
	rm -rf bin
