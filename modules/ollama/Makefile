CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS += -I. -I../../core/stnlabz_abi
PREFIX ?= /opt/digit
MODULE_DIR := $(PREFIX)/modules/ollama
.PHONY: all test install clean
all: bin/ollama.so
bin/ollama.so: ollama.c ollama.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared ollama.c -o $@ -lcurl -ljson-c
bin/test_ollama: test_ollama.c ollama.h ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) test_ollama.c -o $@ -ldl
test: bin/ollama.so bin/test_ollama
	./bin/test_ollama ./bin/ollama.so
install: all
	install -d "$(DESTDIR)$(MODULE_DIR)/bin"
	install -m 0644 module.conf "$(DESTDIR)$(MODULE_DIR)/module.conf"
	install -m 0755 bin/ollama.so "$(DESTDIR)$(MODULE_DIR)/bin/ollama.so"
clean:
	rm -rf bin
