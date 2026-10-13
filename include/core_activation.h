#ifndef DIGIT_CORE_ACTIVATION_H
#define DIGIT_CORE_ACTIVATION_H
#include "stnlabz_abi/abi.h"
#include "core_policy.h"
/* Explicit operator-controlled entrypoints; never invoked by Core startup. */
stnlabz_module_result_t digit_core_authorize(stnlabz_module_registry_t *registry,
    const char *module_id, const digit_policy_request_t *request);
stnlabz_module_result_t digit_core_activate(stnlabz_module_registry_t *registry,
    const char *module_id, const stnlabz_module_host_t *host);
/* Stops active modules, retaining a failed-stop library in memory. */
int digit_core_stop_all(stnlabz_module_registry_t *registry);
#endif
