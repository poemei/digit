#include <stdio.h>
#include <string.h>
#include "core_discovery.h"
#include "logging.h"
static unsigned count, failed;
static void check(int ok, const char *reason)
{
    ++count;
    if (!ok) { ++failed; fprintf(stderr, "FAIL discovery: %s\n", reason); }
}
int main(int argc, char **argv)
{
    stnlabz_module_registry_t registry;
    stnlabz_module_loader_t loader;
    stnlabz_module_discovery_report_t report;
    FILE *f;
    char line[1024];
    int saw_rejection = 0;
    if (argc != 5) return 2;
    if (!digit_log_open_path(argv[4])) return 3;
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    check(digit_core_discover(&registry, &loader, argv[1], &report), "valid scan");
    check(registry.count == 1 && loader.count == 1, "candidate discovered");
    check(registry.modules[0].state == STNLABZ_MODULE_STATE_DISCOVERED &&
          !registry.modules[0].activation_authorized, "no admission or authority");
    digit_core_discovery_cleanup(&registry, &loader);
    check(registry.count == 0 && loader.count == 0, "candidate cleanup");
    check(digit_core_discover(&registry, &loader, argv[2], &report), "invalid manifest scan");
    check(report.modules_rejected == 1 && registry.count == 0,
          "malformed manifest rejected");
    check(digit_core_discover(&registry, &loader, argv[3], &report),
          "missing directory does not prevent Core");
    check(registry.count == 0 && loader.count == 0, "no retained modules");
    check(digit_log_sync(), "logging synchronized");
    digit_log_close();
    f = fopen(argv[4], "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (strstr(line, "DISCOVERY REJECTED_LAST") &&
                strstr(line, "stage=manifest")) saw_rejection = 1;
        fclose(f);
    }
    check(saw_rejection, "rejection stage recorded");
    printf("Core discovery tests: %u executed, %u failed\n", count, failed);
    return failed != 0;
}
