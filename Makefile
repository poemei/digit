CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -ICore -IModel -IView -IController
LDLIBS += -lssl -lcrypto
TARGET := bin/digit
SOURCES := Core/agent_root.c Core/identity_digit.c Model/report.c View/irc_render.c Controller/irc.c Controller/main.c
.PHONY: all clean test
all: $(TARGET)
$(TARGET): $(SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)
test: $(TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) Core/agent_root.c Core/identity_digit.c Model/report.c View/irc_render.c tests/test_core.c -o bin/test_core
	./bin/test_core
clean:
	rm -rf bin
