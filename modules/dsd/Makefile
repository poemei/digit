CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
ROOT := ../..
TARGET := bin/dsd.so
.PHONY: all clean
all: $(TARGET)
$(TARGET): src/dsd.c include/dsd.h
	mkdir -p bin
	$(CC) $(CFLAGS) -Iinclude -I$(ROOT)/include -I$(ROOT)/include/stnlabz_abi -shared src/dsd.c -o $@
clean:
	rm -rf bin
