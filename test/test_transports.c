#include <stdio.h>
#include <string.h>
#include "transports.h"

/* [AI:GPT-6 | 2026-10-19 00:24:01 UTC] Test the transport-neutral ABI dispatch gate. */
static int calls;
static stnlabz_module_result_t dispatch(const char *name,
    const void *request, size_t request_size, void *response,
    size_t response_size, size_t *response_used)
{
    (void)request; (void)request_size; (void)response;
    (void)response_size;
    if (strcmp(name, "transport.send")) return STNLABZ_MODULE_ERR_NOT_FOUND;
    calls++;
    if (response_used) *response_used = 0;
    return STNLABZ_MODULE_OK;
}
int main(void)
{
    stnlabz_module_registry_t registry = {0};
    stnlabz_module_record_t record = {0};
    stnlabz_module_host_t host = {0};
    size_t used = 999;
    host.invoke_service = dispatch;
    registry.modules = &record;
    registry.count = 1;
    strcpy(record.descriptor.id, "transport-example");
    record.state = STNLABZ_MODULE_STATE_QUALIFIED;

    if (digit_transport_invoke(NULL, &host, "transport-example",
        "transport.send", NULL, 0, NULL, 0, &used) !=
        STNLABZ_MODULE_ERR_INVALID_ARGUMENT) return 1;
    if (digit_transport_invoke(&registry, &host, "missing",
        "transport.send", NULL, 0, NULL, 0, &used) !=
        STNLABZ_MODULE_ERR_NOT_FOUND || calls) return 2;
    if (digit_transport_invoke(&registry, &host, "transport-example",
        "transport.send", NULL, 0, NULL, 0, &used) !=
        STNLABZ_MODULE_ERR_INVALID_STATE || calls) return 3;
    record.state = STNLABZ_MODULE_STATE_ACTIVE;
    if (digit_transport_invoke(&registry, &host, "transport-example",
        "transport.send", NULL, 0, NULL, 0, &used) !=
        STNLABZ_MODULE_OK || calls != 1 || used != 0) return 4;
    if (digit_transport_invoke(&registry, &host, "transport-example",
        "absent.service", NULL, 0, NULL, 0, &used) !=
        STNLABZ_MODULE_ERR_NOT_FOUND || calls != 1) return 5;
    puts("Transport ABI boundary tests passed (5 checks)");
    return 0;
}
/* [End AI:GPT-6] */
