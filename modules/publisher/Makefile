CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -I../../core/stnlabz_abi
LDLIBS += -lcurl -ljson-c
PREFIX ?= /opt/digit
TARGET := bin/publisher.so
.PHONY: all test clean install test-login
all: $(TARGET)
$(TARGET): publisher.c ../../core/stnlabz_abi/module.h
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared publisher.c -o $@ $(LDLIBS)
test: $(TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_publisher.c -o bin/test_publisher -ldl
	./bin/test_publisher ./$(TARGET)
install: $(TARGET)
	install -d "$(DESTDIR)$(PREFIX)/modules/publisher/bin"
	install -m 0644 module.conf "$(DESTDIR)$(PREFIX)/modules/publisher/module.conf"
	install -m 0755 $(TARGET) "$(DESTDIR)$(PREFIX)/modules/publisher/bin/publisher.so"
clean:
	rm -rf bin

# Read-only live login acceptance test. Requires private credentials on Digit host.
bin/test_site_login: test_site_login.c
	@mkdir -p bin
	$(CC) $(CFLAGS) test_site_login.c -o $@ -lcurl
test-login: bin/test_site_login
	./bin/test_site_login

