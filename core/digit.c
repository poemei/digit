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

/* [AI:GPT-6 | 2026-10-18] ABI message contract, shared with IRC. */
#include <string.h>
#include <stdatomic.h>
typedef struct {
    char sender[128], account[128], target[128], message[512];
} digit_message_t;
static stnlabz_module_registry_t *live_registry;
static atomic_flag command_lock = ATOMIC_FLAG_INIT;
static char pending_id[64];
static int pending_action; /* 1 activate, 2 deactivate */

static stnlabz_module_result_t digit_service(const char *name,
    const void *request, size_t request_size, void *response,
    size_t response_size, size_t *response_used)
{
    const digit_message_t *msg;
    char reply[512];
    const char *id = NULL;
    int action = 0, n;
    size_t i;
    if (response_used) *response_used = 0;
    if (!name || strcmp(name, "digit.message"))
        return STNLABZ_MODULE_ERR_NOT_FOUND;
    if (!request || request_size != sizeof(digit_message_t) ||
        !response || !response_used || !live_registry)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    msg = request;
    if (!memchr(msg->sender, 0, sizeof msg->sender) ||
        !memchr(msg->account, 0, sizeof msg->account) ||
        !memchr(msg->target, 0, sizeof msg->target) ||
        !memchr(msg->message, 0, sizeof msg->message))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    /* 'target' is the originating channel/nick. Only direct messages
       are passed with target equal to sender by the IRC adapter. */
    if (strcmp(msg->target, msg->sender) ||
        strcmp(msg->account, "STN_Boss")) {
        snprintf(reply, sizeof reply, "Module commands require authenticated STN_Boss DM.");
    } else if (!strcmp(msg->message, "show modules")) {
        size_t used = 0;
        reply[0] = 0;
        if (atomic_flag_test_and_set(&command_lock))
            snprintf(reply, sizeof reply, "Module inventory busy; retry.");
        else {
            for (i = 0; i < live_registry->count; ++i) {
                const stnlabz_module_record_t *entry = &live_registry->modules[i];
                n = snprintf(reply + used, sizeof reply - used,
                    "%s%s=%s (%u/%u)", used ? "; " : "",
                    entry->descriptor.id,
                    stnlabz_module_state_string(entry->state),
                    entry->qualification.tests_passed,
                    entry->qualification.tests_executed);
                if (n < 0 || (size_t)n >= sizeof reply - used) break;
                used += (size_t)n;
            }
            if (!used) snprintf(reply, sizeof reply, "No modules discovered.");
            atomic_flag_clear(&command_lock);
        }
    } else {
        if (!strncmp(msg->message, "activate module ", 16)) {
            id = msg->message + 16;
            action = 1;
        } else if (!strncmp(msg->message, "deactivate module ", 18)) {
            id = msg->message + 18;
            action = 2;
        }
        if (!id || !*id || strlen(id) >= sizeof pending_id ||
            strchr(id, ' ') || strchr(id, '\\r') || strchr(id, '\\n')) {
            snprintf(reply, sizeof reply,
                "Commands: show modules | activate module <id> | deactivate module <id>");
        } else if (atomic_flag_test_and_set(&command_lock)) {
            snprintf(reply, sizeof reply, "Module command busy; retry.");
        } else {
            const stnlabz_module_record_t *entry =
                stnlabz_module_registry_find(live_registry, id);
            if (!entry) snprintf(reply, sizeof reply, "Unknown module: %.60s", id);
            else if (pending_action) snprintf(reply, sizeof reply, "Command already pending.");
            else {
                snprintf(pending_id, sizeof pending_id, "%s", id);
                pending_action = action;
                snprintf(reply, sizeof reply, "%s requested for %.60s.",
                         action == 1 ? "Activation" : "Deactivation", id);
            }
            atomic_flag_clear(&command_lock);
        }
    }
    n = snprintf(response, response_size, "%s", reply);
    if (n < 0 || (size_t)n >= response_size)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    *response_used = (size_t)n;
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_host_t startup_host = {
    .invoke_service = digit_service
};

typedef struct {
    stnlabz_hotload_t *watch;
    stnlabz_module_registry_t *modules;
    const stnlabz_policy_request_t *standing;
} digit_poll_context_t;
static void digit_poll(void *opaque)
{
    digit_poll_context_t *context = opaque;
    char id[64] = {0};
    int action = 0;
    if (!context) return;
    if (!atomic_flag_test_and_set(&command_lock)) {
        action = pending_action;
        if (action) {
            memcpy(id, pending_id, sizeof id);
            pending_action = 0;
        }
        atomic_flag_clear(&command_lock);
    }
    if (action) {
        stnlabz_policy_request_t request = *context->standing;
        stnlabz_module_result_t rc = STNLABZ_MODULE_ERR_INVALID_STATE;
        char detail[256];
        request.module_id = id;
        if (action == 1) {
            rc = stnlabz_core_authorize(context->modules, id, &request);
            if (rc == STNLABZ_MODULE_OK)
                rc = stnlabz_core_activate(context->modules, id,
                    &startup_host, &request);
        } else if (action == 2) {
            stnlabz_policy_result_t decision;
            request.operation = "module.deactivate";
            if (stnlabz_core_policy_evaluate(&request, &decision) &&
                decision.decision == STNLABZ_POLICY_ALLOW &&
                stnlabz_core_stop(context->modules, id))
                rc = STNLABZ_MODULE_OK;
        }
        snprintf(detail, sizeof detail,
                 "module=%.63s action=%s result=%s",
                 id, action == 1 ? "activate" : "deactivate",
                 stnlabz_module_result_string(rc));
        (void)stnlabz_log_event("MODULE", "DM_COMMAND_RESULT", detail);
        (void)stnlabz_log_sync();
    }
    stnlabz_hotload_poll(context->watch);
}

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
    digit_poll_context_t poll_context;
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
    live_registry = &modules;
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
