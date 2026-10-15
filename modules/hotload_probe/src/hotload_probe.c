#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"

#ifndef HOTLOAD_PROBE_PATCH
#define HOTLOAD_PROBE_PATCH 0
#endif

/* Disposable lifecycle probe. No commands, routes, services or network access. */
static int running;
static const stnlabz_module_host_t *runtime_host;

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *result)
{
    if (!result) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->tests_executed = STNLABZ_MODULE_MIN_TESTS;
    result->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    result->negative_test_executed = 1;
    result->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    char message[128];
    if (!host || running) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    runtime_host = host;
    running = 1;
    if (host->send_message) {
        snprintf(message, sizeof(message),
            "[HOTLOAD_PROBE] START version=0.1.%u", (unsigned)HOTLOAD_PROBE_PATCH);
        (void)host->send_message(message);
    }
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t stop(void)
{
    if (!running) return STNLABZ_MODULE_ERR_INVALID_STATE;
    if (runtime_host && runtime_host->send_message)
        (void)runtime_host->send_message("[HOTLOAD_PROBE] STOP");
    running = 0;
    runtime_host = NULL;
    return STNLABZ_MODULE_OK;
}

const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    static stnlabz_module_descriptor_t descriptor;
    memset(&descriptor, 0, sizeof(descriptor));
    snprintf(descriptor.id, sizeof(descriptor.id), "hotload_probe");
    snprintf(descriptor.name, sizeof(descriptor.name), "Digit Hotload Test Probe");
    descriptor.version_major = 0;
    descriptor.version_minor = 1;
    descriptor.version_patch = HOTLOAD_PROBE_PATCH;
    descriptor.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    descriptor.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    descriptor.qualify = qualify;
    descriptor.start = start;
    descriptor.stop = stop;
    return &descriptor;
}
