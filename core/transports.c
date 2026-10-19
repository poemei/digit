#include <string.h>
#include "transports.h"

/* [AI:GPT-6 | 2026-10-19 00:24:01 UTC]
 * Require a live, authorized ABI module before routing to its named service.
 * The host ABI remains responsible for service ownership and dispatch.
 */
stnlabz_module_result_t digit_transport_invoke(
    const stnlabz_module_registry_t *registry,
    const stnlabz_module_host_t *host,
    const char *module_id,
    const char *service,
    const void *request,
    size_t request_size,
    void *response,
    size_t response_size,
    size_t *response_used)
{
    const stnlabz_module_record_t *record;

    if (!registry || !host || !module_id || !*module_id ||
        !service || !*service || !host->invoke_service ||
        (request_size && !request) || (response_size && !response))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;

    if (response_used) *response_used = 0;
    record = stnlabz_module_registry_find(registry, module_id);
    if (!record) return STNLABZ_MODULE_ERR_NOT_FOUND;
    if (record->state != STNLABZ_MODULE_STATE_ACTIVE)
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    return host->invoke_service(service, request, request_size,
                                response, response_size, response_used);
}
/* [End AI:GPT-6] */
