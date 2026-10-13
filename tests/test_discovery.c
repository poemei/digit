#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "core_discovery.h"
#include "logging.h"
static unsigned count, failed;
static stnlabz_module_result_t never_qualify(stnlabz_module_qualification_result_t *q)
{
    (void)q;
    return STNLABZ_MODULE_ERR_QUALIFICATION;
}
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
    int saw_rejection = 0, saw_pass = 0, saw_verification_reject = 0;
    int saw_created = 0, saw_dir_failure = 0;
    if (argc != 5) return 2;
    if (!digit_log_open_path(argv[4])) return 3;
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    check(digit_core_discover(&registry, &loader, argv[1], &report), "valid scan");
    check(registry.count == 1 && loader.count == 1, "candidate discovered");
    check(registry.modules[0].state == STNLABZ_MODULE_STATE_DISCOVERED &&
          !registry.modules[0].activation_authorized, "no admission or authority");
    check(digit_core_verify(&registry), "valid ABI verification");
    check(registry.modules[0].state == STNLABZ_MODULE_STATE_UNVERIFIED &&
          !registry.modules[0].activation_authorized,
          "verified descriptor remains unqualified");
    digit_core_discovery_cleanup(&registry, &loader);
    check(registry.count == 0 && loader.count == 0, "verified candidate cleanup");
    check(digit_core_discover(&registry, &loader, argv[2], &report), "invalid manifest scan");
    check(report.modules_rejected == 1 && registry.count == 0,
          "malformed manifest rejected");
    check(digit_core_verify(&registry), "empty registry verification");
    check(digit_core_discover(&registry, &loader, argv[3], &report),
          "missing directory does not prevent Core");
    {
        struct stat st;
        check(stat(argv[3], &st) == 0 && S_ISDIR(st.st_mode),
              "missing modules directory created");
        check(digit_core_discover(&registry, &loader, argv[3], &report),
              "existing directory reused");
        check(report.modules_discovered == 0, "empty directory scan");
        check(!digit_core_discover(&registry, &loader, argv[4], &report),
              "ordinary file is not a modules directory");
    }
    check(registry.count == 0 && loader.count == 0, "no retained modules");
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    {
        stnlabz_module_descriptor_t descriptor;
        memset(&descriptor, 0, sizeof descriptor);
        strcpy(descriptor.id, "bad-abi");
        strcpy(descriptor.name, "Unsupported ABI");
        descriptor.required_core_api_major = STNLABZ_MODULE_API_MAJOR + 1;
        /* Discovery API enforces callback presence, verification only checks ABI. */
        descriptor.qualify = never_qualify; /* verification must never call this */
        check(stnlabz_module_registry_discover(&registry, &descriptor) ==
              STNLABZ_MODULE_OK, "register incompatible mock descriptor");
        check(digit_core_verify(&registry), "reject candidate without stopping Core");
        check(registry.modules[0].state == STNLABZ_MODULE_STATE_FAILED,
              "ABI mismatch fails verification");
        check(!registry.modules[0].activation_authorized,
              "failed candidate has no authority");
        digit_core_discovery_cleanup(&registry, &loader);
        check(registry.count == 0, "failed verification cleanup");
    }
    check(digit_log_sync(), "logging synchronized");
    digit_log_close();
    char module_path[2048];
    snprintf(module_path,sizeof module_path,"%s",argv[4]);
    char *slash=strrchr(module_path,'/');
    if(slash)strcpy(slash+1,"modules.log");
    f = fopen(module_path, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (strstr(line, "MODULE REJECTED_LAST") &&
                strstr(line, "stage=manifest")) saw_rejection = 1;
            if (strstr(line, "MODULE VERIFY_PASS") &&
                strstr(line, "module=mock")) saw_pass = 1;
            if (strstr(line, "MODULE VERIFY_REJECT") &&
                strstr(line, "module=bad-abi") &&
                strstr(line, "result=INCOMPATIBLE")) saw_verification_reject = 1;
            if (strstr(line, "MODULE SCAN_COMPLETE")) saw_created = 1;
            if (strstr(line, "MODULE DIRECTORY_FAILED") &&
                strstr(line, "not_a_directory")) saw_dir_failure = 1;
        }
        fclose(f);
    }
    check(saw_rejection, "rejection stage recorded");
    check(saw_pass, "successful verification audited");
    check(saw_verification_reject, "incompatible ABI rejection audited");
    check(saw_created, "directory creation audited");
    check(saw_dir_failure, "invalid directory target audited");
    printf("Core discovery tests: %u executed, %u failed\n", count, failed);
    return failed != 0;
}
