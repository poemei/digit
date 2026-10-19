#ifndef DIGIT_TRANSPORTS_H
#define DIGIT_TRANSPORTS_H

#include <stddef.h>
#include "stnlabz_abi/module.h"
#include "stnlabz_abi/module_registry.h"

/* [AI:GPT-6 | 2026-10-19 00:24:01 UTC]
 * Core transport adapter. Service names are ABI-registered capabilities;
 * this interface never assumes an IRC, Slack, or other specific provider.
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
    size_t *response_used);
/* [End AI:GPT-6] */

#endif
