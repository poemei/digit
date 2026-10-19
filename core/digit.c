/* [AI:GPT-6 | 2026-10-18] Operator-approved startup activation.
 * The verified Core bootstrap carries STN_Boss's standing approval for
 * qualified modules. Untrusted IRC messages cannot construct this context.
 */
#include <stdio.h>
#include "agent_root.h"
#include "digit.h"
#include "logging.h"
#include "runtime.h"
#include "core_discovery.h"
#include "core_activation.h"
#include "core_inventory.h"
#include "core_hotload.h"
#include "stnlabz_abi/module_registry.h"
#include "stnlabz_abi/module_loader.h"

/* A service must be registered and authorized before message processing.
 * Do not silently accept unimplemented Core services. */
static stnlabz_module_result_t service_unavailable(const char *name,
    const void *request, size_t request_size, void *response,
    size_t response_size, size_t *response_used)
{
    (void)name; (void)request; (void)request_size;
    (void)response; (void)response_size;
    if (response_used) *response_used = 0;
    return STNLABZ_MODULE_ERR_NOT_FOUND;
}

static const stnlabz_module_host_t startup_host = {
    .invoke_service = service_unavailable
};

static int startup_activate(stnlabz_module_registry_t *modules,
    const stnlabz_policy_request_t *standing)
{
    size_t i;
    int success = 1;
    for (i = 0; i < modules->count; ++i) {
        stnlabz_module_record_t *entry = &modules->modules[i];
        stnlabz_policy_request_t request = *standing;
        stnlabz_module_result_t rc;
        char detail[192];
        if (entry->state != STNLABZ_MODULE_STATE_QUALIFIED) continue;
        request.module_id = entry->descriptor.id;
        rc = stnlabz_core_authorize(modules, entry->descriptor.id, &request);
        if (rc == STNLABZ_MODULE_OK)
            rc = stnlabz_core_activate(modules, entry->descriptor.id,
                                      &startup_host, &request);
        (void)snprintf(detail, sizeof detail,
            "module=%.63s result=%s state=%s",
            entry->descriptor.id, stnlabz_module_result_string(rc),
            stnlabz_module_state_string(entry->state));
        if (!stnlabz_log_event("MODULE",
            rc == STNLABZ_MODULE_OK ? "STARTUP_ACTIVE" : "STARTUP_ACTIVATION_FAILED",
            detail) || !stnlabz_log_sync()) success = 0;
    }
    return success;
}

int stnlabz_initialize(void)
{
    stnlabz_runtime_t runtime;
    stnlabz_module_registry_t modules;
    stnlabz_module_loader_t loader;
    stnlabz_module_discovery_report_t discovery;
    stnlabz_hotload_t *watch = NULL;
    char path[1024];
    const stnlabz_agent_root_t *root = stnlabz_agent_root_identity();
    /* Standing authorization applies to Core startup and hotload only.
       It is not an authentication assertion about incoming IRC users. */
    const stnlabz_policy_request_t standing = {
        .actor = "STN_Boss",
        .mission = "digit",
        .operation = "module.activate",
        .module_id = NULL,
        .within_mission = 1,
        .complies_with_policy = 1,
        .instruction_authorized = 1,
        .authority_granted = 1,
        .verification_source = "operator-approved-core-startup",
        .context_verified = 1
    };
    int ok = 0;
    stnlabz_module_registry_init(&modules);
    stnlabz_module_loader_init(&loader);
    if (!stnlabz_log_open()) return 1;
    if (!stnlabz_agent_root_validate(root)) {
        (void)stnlabz_log_event("CORE", "ROOT_REJECTED", "validation=failed");
        goto done;
    }
    if (!stnlabz_log_event("CORE", "ROOT_VERIFIED", "agent=digit revision=1"))
        goto done;
    stnlabz_runtime_init(&runtime);
    if (stnlabz_module_discovery_get_path(path, sizeof path) != STNLABZ_MODULE_OK ||
        !stnlabz_core_discover(&modules, &loader, path, &discovery) ||
        !stnlabz_core_verify(&modules) ||
        !stnlabz_core_qualify(&modules) ||
        !startup_activate(&modules, &standing) ||
        !stnlabz_core_inventory_log(&modules)) goto done;
    watch = stnlabz_hotload_create(path, &modules, &loader, &startup_host, &standing);
    if (!watch) {
        (void)stnlabz_log_event("MODULE", "HOTLOAD_FAILED",
                               "reason=watch_initialization_failed");
        goto done;
    }
    runtime.module_poll = (void (*)(void *))stnlabz_hotload_poll;
    runtime.module_poll_context = watch;
    puts("STN-LABZ Digit | Core verified");
    if (stnlabz_log_sync() && stnlabz_runtime_run(&runtime)) ok = 1;
done:
    if (watch) stnlabz_hotload_destroy(watch);
    (void)stnlabz_core_stop_all(&modules);
    stnlabz_core_discovery_cleanup(&modules, &loader);
    stnlabz_log_close();
    return ok ? 0 : 1;
}
