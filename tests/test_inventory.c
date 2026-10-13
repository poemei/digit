#include <stdio.h>
#include <string.h>
#include "core_inventory.h"
#include "logging.h"

static unsigned checks, failed, starts;
static void check(int ok, const char *message)
{
    ++checks;
    if (!ok) { ++failed; fprintf(stderr, "FAIL inventory: %s\n", message); }
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    memset(q, 0, sizeof(*q));
    q->tests_executed = q->tests_passed = STNLABZ_MODULE_MIN_TESTS;
    q->negative_test_executed = q->negative_test_passed = 1;
    return STNLABZ_MODULE_OK;
}
int main(int argc, char **argv)
{
    stnlabz_module_registry_t registry;
    stnlabz_module_descriptor_t descriptor;
    digit_core_inventory_entry_t entry, old;
    FILE *f;
    char line[1024];
    unsigned i, entries = 0;
    if (argc != 2) return 2;
    stnlabz_module_registry_init(&registry);
    if (!digit_log_open_path(argv[1])) return 3;
    check(digit_core_inventory_count(&registry) == 0, "empty count");
    check(!digit_core_inventory_get(&registry, 0, &entry), "empty index rejected");
    check(!digit_core_inventory_find(&registry, "missing", &entry), "missing id rejected");
    check(digit_core_inventory_log(&registry), "empty snapshot");
    memset(&descriptor, 0, sizeof descriptor);
    strcpy(descriptor.name, "Inventory test");
    descriptor.required_core_api_major = STNLABZ_MODULE_API_MAJOR;
    descriptor.required_core_api_minor = STNLABZ_MODULE_API_MINOR;
    descriptor.version_major = 2; descriptor.version_minor = 7;
    descriptor.version_patch = 4;
    descriptor.qualify = qualify;
    for (i = 0; i < 65; ++i) {
        snprintf(descriptor.id, sizeof descriptor.id, "inv-%03u", i);
        check(stnlabz_module_registry_discover(&registry, &descriptor) ==
              STNLABZ_MODULE_OK, "registration");
    }
    check(registry.count == 65 && registry.capacity >= 65, "dynamic count");
    check(digit_core_inventory_get(&registry, 0, &old), "initial snapshot");
    check(old.state == STNLABZ_MODULE_STATE_DISCOVERED && !old.active,
          "initial state");
    for (i = 0; i < 65; ++i) {
        char id[64];
        snprintf(id, sizeof id, "inv-%03u", i);
        check(digit_core_inventory_find(&registry, id, &entry), "lookup beyond 32");
        check(!strcmp(entry.id, id) && entry.version_major == 2 &&
              entry.version_minor == 7 && entry.version_patch == 4 &&
              entry.required_api_major == STNLABZ_MODULE_API_MAJOR &&
              entry.required_api_minor == STNLABZ_MODULE_API_MINOR &&
              entry.state == STNLABZ_MODULE_STATE_DISCOVERED, "value copied");
    }
    check(!digit_core_inventory_get(&registry, 65, &entry), "bounds checked");
    check(stnlabz_module_registry_verify(&registry, "inv-000") == STNLABZ_MODULE_OK,
          "verification");
    check(stnlabz_module_registry_qualify(&registry, "inv-000") == STNLABZ_MODULE_OK,
          "qualification");
    check(digit_core_inventory_find(&registry, "inv-000", &entry) &&
          entry.state == STNLABZ_MODULE_STATE_QUALIFIED &&
          entry.qualification.tests_passed == STNLABZ_MODULE_MIN_TESTS &&
          entry.qualification.negative_test_passed && !entry.active &&
          !entry.activation_authorized, "qualified but inactive");
    check(stnlabz_module_registry_authorize_activation(&registry, "inv-000") ==
          STNLABZ_MODULE_OK, "authorization");
    check(digit_core_inventory_find(&registry, "inv-000", &entry) &&
          entry.activation_authorized && !entry.active, "authorized not active");
    check(stnlabz_module_registry_activate(&registry, "inv-000") == STNLABZ_MODULE_OK,
          "activation");
    check(digit_core_inventory_find(&registry, "inv-000", &entry) &&
          entry.active && entry.state == STNLABZ_MODULE_STATE_ACTIVE,
          "active observation");
    check(stnlabz_module_registry_stop(&registry, "inv-000") == STNLABZ_MODULE_OK,
          "stopped");
    check(digit_core_inventory_find(&registry, "inv-000", &entry) &&
          entry.state == STNLABZ_MODULE_STATE_STOPPED && !entry.active &&
          !entry.activation_authorized &&
          entry.qualification.tests_passed == STNLABZ_MODULE_MIN_TESTS,
          "stopped evidence");
    check(stnlabz_module_registry_fail(&registry, "inv-001") == STNLABZ_MODULE_OK,
          "failure state setup");
    check(digit_core_inventory_find(&registry, "inv-001", &entry) &&
          entry.state == STNLABZ_MODULE_STATE_FAILED &&
          !strcmp(entry.failure_reason, "not_recorded_in_registry"),
          "unknown failure cause labelled accurately");
    check(digit_core_inventory_log(&registry), "mixed snapshot");
    check(old.state == STNLABZ_MODULE_STATE_DISCOVERED, "prior copy stays immutable");
    check(starts == 0, "inventory never starts module");
    digit_log_close();
    f = fopen(argv[1], "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (strstr(line, "INVENTORY ENTRY id=inv-")) ++entries;
        fclose(f);
    }
    check(entries == 65, "all 65 inventory records audited");
    stnlabz_module_registry_destroy(&registry);
    printf("Core inventory tests: %u executed, %u failed\n", checks, failed);
    return failed != 0;
}
