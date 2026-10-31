CC ?= cc
ROOT ?= ../..
CPPFLAGS += -Iinclude -I$(ROOT)/include
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS ?= -lcurl -ljson-c -lpthread
.PHONY: all test clean
all: build/research.so
build/research.so: src/research.c include/research.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -o $@ $< $(LDLIBS)
build/test_research: tests/test_research.c include/research.h src/research.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DRESEARCH_TEST -o $@ $< $(LDLIBS)
test: build/test_research
	./build/test_research
clean:
	rm -rf build
