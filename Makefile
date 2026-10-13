CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude -Iinclude/stnlabz_abi

.PHONY: all test clean
all: bin/digit bin/test_agent_root bin/test_runtime bin/test_module_manager bin/test_logging bin/test_abi_compat bin/test_loader

bin:
	mkdir -p bin

bin/digit: src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c src/logging.c include/logging.h include/digit.h include/runtime.h include/agent_root.h include/stnlabz_abi/module.h include/stnlabz_abi/module_registry.h include/stnlabz_abi/abi.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c src/logging.c -o $@

bin/test_agent_root: tests/test_agent_root.c src/agent_root.c src/identity_digit.c include/agent_root.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_agent_root.c src/agent_root.c src/identity_digit.c -o $@

bin/test_runtime: tests/test_runtime.c src/runtime.c src/logging.c include/runtime.h include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_runtime.c src/runtime.c src/logging.c -o $@

bin/test_module_manager: tests/test_module_manager.c src/module_manager.c src/logging.c include/module_manager.h include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_module_manager.c src/module_manager.c src/logging.c -o $@

bin/test_logging: tests/test_logging.c src/logging.c include/logging.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_logging.c src/logging.c -o $@

bin/test_abi_compat: tests/test_abi_compat.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c include/stnlabz_abi/module.h include/stnlabz_abi/module_registry.h include/stnlabz_abi/abi.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_abi_compat.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c -o $@

bin/test_loader: tests/test_loader.c src/stnlabz_abi/loader_linux.c src/stnlabz_abi/module_registry.c include/stnlabz_abi/module_loader.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_loader.c src/stnlabz_abi/loader_linux.c src/stnlabz_abi/module_registry.c -ldl -o $@

bin/loader_valid.so: tests/fixtures/abi_loader_fixture.c include/stnlabz_abi/module.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DTEST_EXPORT -DTEST_MODULE_ID='"mock"' tests/fixtures/abi_loader_fixture.c -o $@

bin/loader_legacy.so: tests/fixtures/abi_loader_fixture.c include/stnlabz_abi/module.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared -DTEST_MODULE_ID='"mock"' tests/fixtures/abi_loader_fixture.c -o $@

test: all bin/loader_valid.so bin/loader_legacy.so
	@sh tests/run_all.sh

clean:
	rm -rf bin
