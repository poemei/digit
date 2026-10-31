#include <stdio.h>
#include <string.h>
#include "research.h"

/* ABI 1.5 module entry point, exported by research.c. */
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);

static unsigned commands_registered, commands_removed;
static unsigned services_registered, services_removed;
static int fail_second_service;
static int fail_command;

static int add_command(const char *name, stnlabz_module_command_handler_fn handler, void *ctx)
{
    (void)ctx;
    if (!name || strcmp(name, "research") || !handler || fail_command) return 0;
    ++commands_registered;
    return 1;
}
static int remove_command(const char *name, void *ctx)
{
    (void)ctx;
    if (!name || strcmp(name, "research")) return 0;
    ++commands_removed;
    return 1;
}
static int add_service(const char *name, stnlabz_module_service_handler_fn handler, void *ctx)
{
    (void)ctx;
    if (!name || !handler) return 0;
    if (strcmp(name, DIGIT_RESEARCH_ANSWER_SERVICE) &&
        strcmp(name, DIGIT_RESEARCH_SEARCH_SERVICE)) return 0;
    if (fail_second_service && !strcmp(name, DIGIT_RESEARCH_SEARCH_SERVICE)) return 0;
    ++services_registered;
    return 1;
}
static int remove_service(const char *name, void *ctx)
{
    (void)ctx;
    if (!name || (strcmp(name, DIGIT_RESEARCH_ANSWER_SERVICE) &&
                  strcmp(name, DIGIT_RESEARCH_SEARCH_SERVICE))) return 0;
    ++services_removed;
    return 1;
}

int main(void)
{
    const stnlabz_module_descriptor_t *module = stnlabz_module_get_descriptor();
    stnlabz_module_qualification_result_t q = {0};
    stnlabz_module_host_t host = {0};
    unsigned checks = 0, passed = 0;
#define CHECK(expr) do { ++checks; if (expr) ++passed; else \
    fprintf(stderr, "ABI check failed at line %d: %s\n", __LINE__, #expr); } while (0)

    CHECK(module != NULL);
    if (!module) return 1;
    CHECK(!strcmp(module->id, "research"));
    CHECK(module->required_core_api_major == STNLABZ_MODULE_API_MAJOR);
    CHECK(module->required_core_api_minor == STNLABZ_MODULE_API_MINOR);
    CHECK(module->version_major == 1);
    CHECK(module->qualify != NULL);
    CHECK(module->start != NULL);
    CHECK(module->stop != NULL);
    CHECK(module->qualify(NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    CHECK(module->qualify(&q) == STNLABZ_MODULE_OK);
    CHECK(q.tests_executed >= STNLABZ_MODULE_MIN_TESTS);
    CHECK(q.tests_passed == q.tests_executed);
    CHECK(q.tests_failed == 0);
    CHECK(q.negative_test_executed && q.negative_test_passed);
    CHECK(module->start(NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT);

    host.register_command = add_command;
    host.unregister_command = remove_command;
    host.register_service = add_service;
    host.unregister_service = remove_service;

    CHECK(module->start(&host) == STNLABZ_MODULE_OK);
    CHECK(commands_registered == 1);
    CHECK(services_registered == 2);
    CHECK(module->stop() == STNLABZ_MODULE_OK);
    CHECK(commands_removed == 1);
    CHECK(services_removed == 2);

    fail_second_service = 1;
    CHECK(module->start(&host) == STNLABZ_MODULE_ERR_START_FAILED);
    CHECK(commands_registered == 1);
    CHECK(services_registered == 3);
    CHECK(services_removed == 3);
    fail_second_service = 0;

    fail_command = 1;
    CHECK(module->start(&host) == STNLABZ_MODULE_ERR_START_FAILED);
    CHECK(services_registered == 5);
    CHECK(services_removed == 5);
    fail_command = 0;

    CHECK(module->start(&host) == STNLABZ_MODULE_OK);
    CHECK(module->stop() == STNLABZ_MODULE_OK);

    printf("Research qualification %u/%u; negative=%d/%d; ABI lifecycle %u/%u %s\n",
        q.tests_passed, q.tests_executed,
        q.negative_test_passed, q.negative_test_executed,
        passed, checks, passed == checks ? "PASS" : "FAIL");
    return passed == checks ? 0 : 1;
#undef CHECK
}
