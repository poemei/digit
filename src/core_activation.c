#include <stdio.h>
#include <string.h>
#include "core_activation.h"
#include "logging.h"

static int audit(const char *event, const char *id,
                 stnlabz_module_result_t result,
                 const stnlabz_module_record_t *record)
{
    char detail[320];
    (void)snprintf(detail, sizeof detail,
        "module=%.63s version=%u.%u.%u result=%s state=%s authorized=%d",
        id ? id : "unknown",
        record ? record->descriptor.version_major : 0,
        record ? record->descriptor.version_minor : 0,
        record ? record->descriptor.version_patch : 0,
        stnlabz_module_result_string(result),
        record ? stnlabz_module_state_string(record->state) : "NOT_FOUND",
        record ? record->activation_authorized : 0);
    return digit_log_event("MODULE", event, detail) && digit_log_sync();
}

stnlabz_module_result_t digit_core_authorize(stnlabz_module_registry_t *registry,
    const char *module_id, const digit_policy_request_t *request)
{
    stnlabz_module_result_t rc;
    digit_policy_result_t decision;
    if (!registry || !module_id || !digit_log_is_open())
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!audit("AUTHORIZE_REQUEST", module_id, STNLABZ_MODULE_OK,
               stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    /* No authority may be inferred from qualification or module presence.
       Only trusted Core callers may supply verified request context. */
    if (!request || !request->operation || !request->module_id ||
        strcmp(request->operation, "module.activate") != 0 ||
        strcmp(request->module_id, module_id) != 0 ||
        !digit_core_policy_evaluate(request, &decision))
        return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    if (decision.decision != DIGIT_POLICY_ALLOW) {
        if (!audit("AUTHORIZE_REJECT", module_id, STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
                   stnlabz_module_registry_find(registry, module_id)))
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    }
    rc = stnlabz_module_registry_authorize_activation(registry, module_id);
    if (!audit(rc == STNLABZ_MODULE_OK ? "AUTHORIZE_GRANTED" : "AUTHORIZE_REJECT",
               module_id, rc, stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    return rc;
}

stnlabz_module_result_t digit_core_activate(stnlabz_module_registry_t *registry,
    const char *module_id, const stnlabz_module_host_t *host,
    const digit_policy_request_t *request)
{
    const stnlabz_module_record_t *record;
    stnlabz_module_result_t rc;
    digit_policy_result_t decision;
    if (!registry || !module_id || !digit_log_is_open())
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    record = stnlabz_module_registry_find(registry, module_id);
    if (!audit("ACTIVATE_REQUEST", module_id, STNLABZ_MODULE_OK, record))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    /* Guard before start(): unauthorized callbacks must never run. */
    if (!record) rc = STNLABZ_MODULE_ERR_NOT_FOUND;
    else if (record->state != STNLABZ_MODULE_STATE_QUALIFIED &&
             record->state != STNLABZ_MODULE_STATE_STOPPED)
        rc = STNLABZ_MODULE_ERR_NOT_QUALIFIED;
    else if (!record->activation_authorized)
        rc = STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    else if (!request || !request->operation || !request->module_id ||
             strcmp(request->operation, "module.activate") != 0 ||
             strcmp(request->module_id, module_id) != 0)
        rc = STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    else if (!digit_core_policy_evaluate(request, &decision))
        rc = STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    else if (decision.decision != DIGIT_POLICY_ALLOW)
        rc = STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
    else {
        /* The shared ABI owns the start->active transition and failure handling.
           Its combined helper repeats authorization only AFTER the explicit gate. */
        rc = stnlabz_module_abi_authorize_and_activate(registry, module_id, host);
    }
    if (!audit(rc == STNLABZ_MODULE_OK ? "HOTLOAD_ACTIVE" : "ACTIVATE_REJECT",
               module_id, rc, stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    return rc;
}

int digit_core_stop(stnlabz_module_registry_t *registry, const char *module_id)
{
    const stnlabz_module_record_t *record;
    stnlabz_module_result_t rc;
    if (!registry || !module_id || !digit_log_is_open()) return 0;
    record = stnlabz_module_registry_find(registry, module_id);
    if (!record || record->state != STNLABZ_MODULE_STATE_ACTIVE) return 0;
    if (!audit("STOP_REQUEST", module_id, STNLABZ_MODULE_OK, record))
        return 0;
    rc = stnlabz_module_abi_stop(registry, module_id);
    if (!audit(rc == STNLABZ_MODULE_OK ? "STOP_PASS" : "STOP_FAILED",
               module_id, rc, stnlabz_module_registry_find(registry, module_id)))
        return 0;
    return rc == STNLABZ_MODULE_OK;
}

int digit_core_stop_all(stnlabz_module_registry_t *registry)
{
    size_t i;
    int good = 1;
    if (!registry || !digit_log_is_open()) return 0;
    for (i = registry->count; i > 0; --i) {
        stnlabz_module_record_t *record = &registry->modules[i - 1];
        if (record->state != STNLABZ_MODULE_STATE_ACTIVE) continue;
        if (!digit_core_stop(registry, record->descriptor.id)) good = 0;
    }
    return good;
}
