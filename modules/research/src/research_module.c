#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "research_internal.h"

#define RESEARCH_VERSION_MAJOR 1U
#define RESEARCH_VERSION_MINOR 0U
#define RESEARCH_VERSION_PATCH 1U
#define RESEARCH_ROOT "/opt/digit/research/sources"
#define RESEARCH_SERVICE "digit.research.lookup"

typedef struct {
    char topic[64];
    char terms[RESEARCH_INPUT_MAX];
} research_request_t;
typedef struct {
    char text[RESEARCH_OUTPUT_MAX];
    unsigned int verified;
} research_response_t;

static const stnlabz_module_host_t *owner;
static int registered;

/* Service is a capability, not a chat command. The caller supplies structured
 * fields; this module does not interpret conversation or govern IRC routing. */
static stnlabz_module_result_t lookup(
    const void *request, size_t request_size,
    void *response, size_t response_size, size_t *response_used,
    void *handler_context)
{
    const research_request_t *in = request;
    research_response_t *out = response;
    (void)handler_context;
    if (response_used) *response_used = 0;
    if (!in || request_size != sizeof(*in) || !out ||
        response_size < sizeof(*out) ||
        !memchr(in->topic, 0, sizeof in->topic) ||
        !memchr(in->terms, 0, sizeof in->terms) ||
        !research_topic_valid(in->topic) || !in->terms[0])
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;

    memset(out, 0, sizeof(*out));
    if (!research_search(RESEARCH_ROOT, in->topic, in->terms,
                         out->text, sizeof out->text))
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    /* All retrieved text is unverified, regardless of source wording. */
    out->verified = 0U;
    if (response_used) *response_used = sizeof(*out);
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *out)
{
    unsigned int tests = 0, passed = 0;
    int neg = 0, negpass = 0, ok;
    if (!out) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof *out);
    ok = research_selftests(&tests, &passed, &neg, &negpass);
    out->tests_executed = tests;
    out->tests_passed = passed;
    out->tests_failed = tests - passed;
    out->negative_test_executed = neg;
    out->negative_test_passed = negpass;
    return ok ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_QUALIFICATION;
}

static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    if (!host || !host->register_service || !host->unregister_service)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!host->register_service(RESEARCH_SERVICE, lookup, NULL))
        return STNLABZ_MODULE_ERR_START_FAILED;
    owner = host;
    registered = 1;
    fprintf(stderr, "[RESEARCH] service active: %s; no chat commands registered\n",
            RESEARCH_SERVICE);
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t stop(void)
{
    if (registered && owner)
        (void)owner->unregister_service(RESEARCH_SERVICE, NULL);
    registered = 0;
    owner = NULL;
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t descriptor = {
    "research", "Digit Research",
    RESEARCH_VERSION_MAJOR, RESEARCH_VERSION_MINOR, RESEARCH_VERSION_PATCH,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};

const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
