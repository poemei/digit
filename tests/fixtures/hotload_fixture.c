#include <string.h>
#include "stnlabz_abi/module.h"

#ifndef FIXTURE_VERSION
#define FIXTURE_VERSION 0
#endif
#ifndef FIXTURE_BAD_QUAL
#define FIXTURE_BAD_QUAL 0
#endif
#ifndef FIXTURE_BAD_START
#define FIXTURE_BAD_START 0
#endif

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r)
{
    if (!r) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(r, 0, sizeof *r);
    r->tests_executed = FIXTURE_BAD_QUAL ? 9U : 10U;
    r->tests_passed = r->tests_executed;
    r->negative_test_executed = 1;
    r->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
#ifdef FIXTURE_REGISTER_SERVICE
#include <stdatomic.h>
#include <sched.h>
typedef struct {
    atomic_int entered;
    atomic_int release;
    atomic_int completed;
} fixture_block_t;
static const stnlabz_module_host_t *service_host;
static int service_owner;
static stnlabz_module_result_t blocking_service(
    const void *request, size_t request_size, void *response,
    size_t response_size, size_t *response_used, void *context)
{
    fixture_block_t *state;
    (void)response; (void)response_size; (void)response_used;
    if (!request || request_size != sizeof(fixture_block_t *) ||
        context != &service_owner) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memcpy(&state, request, sizeof state);
    if (!state) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    atomic_store(&state->entered, 1);
    while (!atomic_load(&state->release)) (void)sched_yield();
    atomic_store(&state->completed, 1);
    return STNLABZ_MODULE_OK;
}
#endif
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    if (FIXTURE_BAD_START) return STNLABZ_MODULE_ERR_START_FAILED;
#ifdef FIXTURE_REGISTER_SERVICE
    if (!host || !host->register_service ||
        !host->register_service("fixture.block", blocking_service, &service_owner))
        return STNLABZ_MODULE_ERR_START_FAILED;
    service_host = host;
#else
    (void)host;
#endif
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
#ifdef FIXTURE_REGISTER_SERVICE
    if (!service_host || !service_host->unregister_service ||
        !service_host->unregister_service("fixture.block", &service_owner))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    service_host = NULL;
#endif
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t module = {
    "fixture", "Core Hotload Test Fixture", 1U, 0U, FIXTURE_VERSION,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &module;
}
