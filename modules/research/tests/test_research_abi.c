#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "research_service.h"

/* Module entry point is exported by research_module.c, not declared by the ABI header. */
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);

static stnlabz_module_service_handler_fn registered_handler;
static void *registered_context;
static unsigned int register_calls, unregister_calls;
static int force_registration_failure;

static int register_service(const char *name,
    stnlabz_module_service_handler_fn handler, void *context)
{
    register_calls++;
    if (force_registration_failure || !name || !handler ||
        strcmp(name, DIGIT_RESEARCH_LOOKUP_SERVICE) || registered_handler)
        return 0;
    registered_handler = handler;
    registered_context = context;
    return 1;
}
static int unregister_service(const char *name, void *context)
{
    unregister_calls++;
    if (!name || strcmp(name, DIGIT_RESEARCH_LOOKUP_SERVICE) ||
        !registered_handler || context != registered_context)
        return 0;
    registered_handler = NULL;
    registered_context = NULL;
    return 1;
}
#define CHECK(x) do { ++total; if (x) ++passed; \
    else fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); } while (0)

int main(void)
{
    unsigned int total=0,passed=0;
    size_t used=123;
    stnlabz_module_result_t rc;
    stnlabz_module_qualification_result_t q;
    digit_research_request_t req;
    digit_research_response_t out;
    stnlabz_module_host_t host = {0};
    const stnlabz_module_descriptor_t *descriptor =
        stnlabz_module_get_descriptor();

    CHECK(descriptor != NULL);
    if (!descriptor) return 1;
    CHECK(strcmp(descriptor->id, "research") == 0);
    CHECK(descriptor->required_core_api_major == STNLABZ_MODULE_API_MAJOR);
    CHECK(descriptor->required_core_api_minor == STNLABZ_MODULE_API_MINOR);
    CHECK(descriptor->version_major == 1U && descriptor->version_minor == 0U &&
          descriptor->version_patch == 1U);
    CHECK(descriptor->qualify(&q) == STNLABZ_MODULE_OK);
    CHECK(q.tests_executed >= STNLABZ_MODULE_MIN_TESTS);
    CHECK(q.tests_failed == 0 && q.negative_test_executed && q.negative_test_passed);
    CHECK(descriptor->start(NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    CHECK(descriptor->start(&host) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);

    host.register_service = register_service;
    host.unregister_service = unregister_service;
    force_registration_failure = 1;
    CHECK(descriptor->start(&host) == STNLABZ_MODULE_ERR_START_FAILED);
    CHECK(registered_handler == NULL);
    force_registration_failure = 0;
    CHECK(descriptor->start(&host) == STNLABZ_MODULE_OK);
    CHECK(registered_handler != NULL);
    CHECK(register_calls == 2);
    CHECK(host.register_command == NULL); /* command system not required */

    memset(&req, 0, sizeof req);
    memset(&out, 0, sizeof out);
    memcpy(req.topic, "security", sizeof "security");
    memcpy(req.terms, "firmware", sizeof "firmware");

    rc = registered_handler(&req, sizeof req, &out, sizeof out, &used,
                            registered_context);
    CHECK(rc == STNLABZ_MODULE_OK);
    CHECK(used == sizeof out);
    CHECK(out.verified == 0U);
    CHECK(strstr(out.text, "UNKNOWN") != NULL || strstr(out.text, "UNVERIFIED") != NULL);

    used = 123;
    CHECK(registered_handler(NULL, 0, &out, sizeof out, &used,
                             registered_context) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    CHECK(used == 0);

    used = 123;
    CHECK(registered_handler(&req, sizeof req - 1, &out, sizeof out, &used,
                             registered_context) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    CHECK(used == 0);

    memset(req.topic, 'x', sizeof req.topic);
    CHECK(registered_handler(&req, sizeof req, &out, sizeof out, &used,
                             registered_context) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    memset(req.topic, 0, sizeof req.topic);
    memcpy(req.topic, "security", sizeof "security");
    memset(req.terms, 0, sizeof req.terms);
    CHECK(registered_handler(&req, sizeof req, &out, sizeof out, &used,
                             registered_context) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);

    CHECK(descriptor->stop() == STNLABZ_MODULE_OK);
    CHECK(registered_handler == NULL);
    CHECK(unregister_calls == 1);
    CHECK(descriptor->stop() == STNLABZ_MODULE_OK);
    CHECK(unregister_calls == 1);

    printf("Research ABI integration: %u/%u PASS\n",passed,total);
    return passed==total ? 0 : 1;
}
