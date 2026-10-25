# [AI:GPT-6 | 2026-10-25 18:20:00 UTC]
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
ABI_DIR ?= ../../core/stnlabz_abi
CPPFLAGS ?= -Iincludes -I$(ABI_DIR)
PREFIX ?= /opt/digit
ST1_PATH ?= $(PREFIX)/research/st1/research.tsv
.PHONY: all test clean install check-abi
all: check-abi bin/st1.so bin/st1_manage
check-abi:
	@test -f "$(ABI_DIR)/module.h" || { echo 'Missing Digit ABI header'; exit 1; }
bin:
	mkdir -p bin
bin/st1.so: src/st1.c includes/st1.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared src/st1.c -o $@
bin/st1_manage: src/st1.c includes/st1.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -DST1_CLI -DST1_PATH='"$(ST1_PATH)"' src/st1.c -o $@
bin/test_st1: tests/test_st1.c src/st1.c includes/st1.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_st1.c -o $@ -ldl
test: all bin/test_st1
	./bin/test_st1
	sh tests/test_st1.sh
install: all
	@test "$$(id -u)" -eq 0 || { echo 'install requires root'; exit 1; }
	install -d -o digit -g digit -m 0750 $(PREFIX)/research/st1
	install -d -o digit -g digit -m 0750 $(PREFIX)/modules/st1/bin
	install -m 0755 bin/st1.so $(PREFIX)/modules/st1/bin/st1.so
	install -m 0644 module.conf $(PREFIX)/modules/st1/module.conf
	install -m 0750 -o digit -g digit bin/st1_manage $(PREFIX)/modules/st1/bin/st1_manage
	@echo 'Installed only; separate human authorization required for activation.'
clean:
	rm -rf bin
# [End AI:GPT-6]
