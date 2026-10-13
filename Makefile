CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude -Iinclude/stnlabz_abi

.PHONY: all test clean install install-modules
all: bin/digit bin/test_agent_root bin/test_runtime bin/test_module_manager bin/test_logging bin/test_abi_compat bin/test_loader bin/test_discovery bin/test_registry_growth bin/test_loader_growth bin/test_qualification bin/test_activation bin/test_inventory bin/test_policy bin/test_provenance bin/test_escalation bin/test_lifecycle_isolation bin/test_core_failure bin/test_authority_boundary bin/test_router bin/test_bootstrap bin/test_model_expression bin/test_rendering

bin:
	mkdir -p bin

bin/digit: src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c src/stnlabz_abi/loader_linux.c src/stnlabz_abi/module_discovery_linux.c src/core_discovery.c src/core_activation.c src/core_inventory.c src/core_policy.c src/logging.c include/core_policy.h include/core_inventory.h include/logging.h include/digit.h include/runtime.h include/agent_root.h include/stnlabz_abi/module.h include/stnlabz_abi/module_registry.h include/stnlabz_abi/abi.h include/stnlabz_abi/module_discovery.h include/core_discovery.h src/mvc_console.c src/bootstrap.c src/router.c src/model_expression.c src/rendering.c include/mvc_console.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/digit.c src/runtime.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/module.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/abi.c src/stnlabz_abi/loader_linux.c src/stnlabz_abi/module_discovery_linux.c src/core_discovery.c src/core_activation.c src/core_inventory.c src/core_policy.c src/logging.c src/mvc_console.c src/bootstrap.c src/router.c src/model_expression.c src/rendering.c -ldl -o $@

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

bin/test_discovery: tests/test_discovery.c src/core_discovery.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/loader_linux.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_discovery.c src/core_discovery.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/loader_linux.c src/logging.c -ldl -o $@

bin/test_registry_growth: tests/test_registry_growth.c src/stnlabz_abi/module_registry.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_registry_growth.c src/stnlabz_abi/module_registry.c -Wl,--wrap=realloc -o $@

bin/test_loader_growth: tests/test_loader_growth.c src/stnlabz_abi/loader_linux.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_loader_growth.c src/stnlabz_abi/loader_linux.c -ldl -Wl,--wrap=realloc -o $@

bin/test_qualification: tests/test_qualification.c src/core_discovery.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/loader_linux.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_qualification.c src/core_discovery.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/loader_linux.c src/logging.c -ldl -o $@

bin/test_activation: tests/test_activation.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/core_discovery.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/loader_linux.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_activation.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/core_discovery.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/stnlabz_abi/module_discovery_linux.c src/stnlabz_abi/loader_linux.c src/logging.c -ldl -o $@

bin/test_inventory: tests/test_inventory.c src/core_inventory.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_inventory.c src/core_inventory.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@

bin/test_policy: tests/test_policy.c src/core_policy.c src/core_activation.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_policy.c src/core_policy.c src/core_activation.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@


install: bin/digit
	@sh scripts/install.sh
	@$(MAKE) install-modules

install-modules:
	@set -eu; for manifest in modules/*/module.conf; do \
		[ -f "$$manifest" ] || continue; \
		dir=$$(dirname "$$manifest"); \
		$(MAKE) -C "$$dir" install PREFIX=/opt/digit OWNER=digit GROUP=digit; \
	done

test: all bin/loader_valid.so bin/loader_legacy.so
	@sh tests/run_all.sh

clean:
	rm -rf bin

bin/test_provenance: tests/test_provenance.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_provenance.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c -o $@

bin/test_escalation: tests/test_escalation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_escalation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c -o $@

bin/test_lifecycle_isolation: tests/test_lifecycle_isolation.c src/stnlabz_abi/module_registry.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_lifecycle_isolation.c src/stnlabz_abi/module_registry.c -o $@

bin/test_core_failure: tests/test_core_failure.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_core_failure.c src/core_policy.c src/agent_root.c src/identity_digit.c src/logging.c -o $@

bin/test_authority_boundary: tests/test_authority_boundary.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_authority_boundary.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@

bin/test_router: tests/test_router.c src/router.c src/logging.c src/stnlabz_abi/module_registry.c include/router.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_router.c src/router.c src/logging.c src/stnlabz_abi/module_registry.c -o $@

bin/test_bootstrap: tests/test_bootstrap.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c include/bootstrap.h include/router.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_bootstrap.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@

bin/test_model_expression: tests/test_model_expression.c src/model_expression.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c include/model_expression.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_model_expression.c src/model_expression.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@

bin/test_rendering: tests/test_rendering.c src/rendering.c src/model_expression.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c include/rendering.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_rendering.c src/rendering.c src/model_expression.c src/bootstrap.c src/router.c src/core_activation.c src/core_policy.c src/agent_root.c src/identity_digit.c src/stnlabz_abi/abi.c src/stnlabz_abi/module_registry.c src/stnlabz_abi/module.c src/logging.c -o $@

