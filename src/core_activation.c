#include <stdio.h>
#include "core_activation.h"
#include "logging.h"

static int audit(const char *event, const char *id,
                 stnlabz_module_result_t result,
                 const stnlabz_module_record_t *record)
{
    char detail[320];
    (void)snprintf(detail, sizeof detail,
        "id=%.63s result=%s state=%s authorized=%d",
        id ? id : "unknown", stnlabz_module_result_string(result),
        record ? stnlabz_module_state_string(record->state) : "NOT_FOUND",
        record ? record->activation_authorized : 0);
    return digit_log_event("ACTIVATION", event, detail) && digit_log_sync();
}

stnlabz_module_result_t digit_core_authorize(stnlabz_module_registry_t *registry,
    const char *module_id)
{
    stnlabz_module_result_t rc;
    if (!registry || !module_id || !digit_log_is_open())
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!audit("AUTHORIZE_REQUEST", module_id, STNLABZ_MODULE_OK,
               stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    rc = stnlabz_module_registry_authorize_activation(registry, module_id);
    if (!audit(rc == STNLABZ_MODULE_OK ? "AUTHORIZE_GRANTED" : "AUTHORIZE_REJECT",
               module_id, rc, stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    return rc;
}

stnlabz_module_result_t digit_core_activate(stnlabz_module_registry_t *registry,
    const char *module_id, const stnlabz_module_host_t *host)
{
    const stnlabz_module_record_t *record;
    stnlabz_module_result_t rc;
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
    else {
        /* The shared ABI owns the start->active transition and failure handling.
           Its combined helper repeats authorization only AFTER the explicit gate. */
        rc = stnlabz_module_abi_authorize_and_activate(registry, module_id, host);
    }
    if (!audit(rc == STNLABZ_MODULE_OK ? "ACTIVATE_PASS" : "ACTIVATE_REJECT",
               module_id, rc, stnlabz_module_registry_find(registry, module_id)))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    return rc;
}

int digit_core_stop_all(stnlabz_module_registry_t *registry)
{
    size_t i;
    int good = 1;
    if (!registry || !digit_log_is_open()) return 0;
    for (i = registry->count; i > 0; --i) {
        stnlabz_module_record_t *record = &registry->modules[i - 1];
        stnlabz_module_result_t rc;
        if (record->state != STNLABZ_MODULE_STATE_ACTIVE) continue;
        if (!audit("STOP_REQUEST", record->descriptor.id, STNLABZ_MODULE_OK, record))
            return 0;
        rc = stnlabz_module_abi_stop(registry, record->descriptor.id);
        if (!audit(rc == STNLABZ_MODULE_OK ? "STOP_PASS" : "STOP_FAILED",
                   record->descriptor.id, rc, record))
            return 0;
        if (rc != STNLABZ_MODULE_OK) good = 0;
    }
    return good;
}
