#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "core_hotload.h"
#include "core_activation.h"
#include "logging.h"

static unsigned tests, passed;
#define CHECK(label, condition) do { \
    int ok = !!(condition); ++tests; passed += (unsigned)ok; \
    printf("%s hotload %s\n", ok ? "PASS" : "FAIL", label); \
} while (0)

static int install_fixture(const char *from, const char *to)
{
    char stage[PATH_MAX];
    char buffer[16384];
    int in = -1, out = -1, ok = 0;
    ssize_t n;
    if (snprintf(stage, sizeof stage, "%s.next.XXXXXX", to) >= (int)sizeof stage)
        return 0;
    in = open(from, O_RDONLY);
    if (in < 0) return 0;
    out = mkstemp(stage);
    if (out < 0) goto done;
    while ((n = read(in, buffer, sizeof buffer)) != 0) {
        ssize_t off = 0;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto done;
        while (off < n) {
            ssize_t w = write(out, buffer + off, (size_t)(n - off));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) goto done;
            off += w;
        }
    }
    if (fsync(out) || close(out)) { out = -1; goto done; }
    out = -1;
    if (rename(stage, to)) goto done;
    ok = 1;
done:
    if (out >= 0) (void)close(out);
    if (in >= 0) (void)close(in);
    if (!ok) (void)unlink(stage);
    return ok;
}

static int active_version(const stnlabz_module_registry_t *registry, unsigned patch)
{
    const stnlabz_module_record_t *r =
        stnlabz_module_registry_find(registry, "fixture");
    return r && r->state == STNLABZ_MODULE_STATE_ACTIVE &&
        r->descriptor.version_major == 1U &&
        r->descriptor.version_minor == 0U &&
        r->descriptor.version_patch == patch;
}
static int log_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[2048];
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, needle)) { (void)fclose(f); return 1; }
    }
    (void)fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    char base[] = "/tmp/digit-hotload-XXXXXX";
    char modules[PATH_MAX], module_dir[PATH_MAX], target[PATH_MAX];
    char staging[PATH_MAX], core_log[PATH_MAX], module_log[PATH_MAX];
    stnlabz_module_registry_t registry;
    stnlabz_module_loader_t loader;
    stnlabz_hotload_t *watch = NULL;
    int initialized = 0;
    if (argc != 5) {
        fputs("usage: test_core_hotload old.so badqual.so badstart.so good.so\n", stderr);
        return 2;
    }
    if (!mkdtemp(base)) { perror("mkdtemp"); return 2; }
#define PATH_SET(dst, fmt, val) do { \
    if (snprintf(dst, sizeof dst, fmt, val) >= (int)sizeof dst) goto cleanup; \
} while (0)
    PATH_SET(modules, "%s/modules", base);
    PATH_SET(module_dir, "%s/modules/fixture", base);
    PATH_SET(target, "%s/modules/fixture/fixture.so", base);
    PATH_SET(staging, "%s/staging", base);
    PATH_SET(core_log, "%s/core.log", base);
    PATH_SET(module_log, "%s/modules.log", base);
    if (mkdir(modules, 0700) || mkdir(module_dir, 0700) ||
        mkdir(staging, 0700)) goto cleanup;
    if (setenv("DIGIT_HOTLOAD_TEST_STAGE", staging, 1) != 0) goto cleanup;
    if (!stnlabz_log_open_path(core_log)) goto cleanup;
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    initialized = 1;
    watch = stnlabz_hotload_create(modules, &registry, &loader, NULL);
    CHECK("watch created", watch != NULL);
    if (!watch) goto cleanup;

    CHECK("initial fixture installed", install_fixture(argv[1], target));
    stnlabz_hotload_poll(watch);
    CHECK("qualified initial fixture activated", active_version(&registry, 0U));
    CHECK("initial qualification audit", log_contains(module_log, "QUALIFICATION_GREEN"));

    CHECK("unqualified update installed", install_fixture(argv[2], target));
    stnlabz_hotload_poll(watch);
    CHECK("unqualified update retains incumbent", active_version(&registry, 0U));
    CHECK("qualification rejected and logged",
          log_contains(module_log, "qualification_failed_incumbent_retained"));
    CHECK("failed update counted in audit", log_contains(module_log, "executed=9"));

    CHECK("start-failing update installed", install_fixture(argv[3], target));
    stnlabz_hotload_poll(watch);
    CHECK("start failure restores incumbent", active_version(&registry, 0U));
    CHECK("start failure audit present",
          log_contains(module_log, "replacement_activation_failed"));
    CHECK("restoration audit present",
          log_contains(module_log, "HOTLOAD_ROLLBACK_ACTIVE"));

    CHECK("qualified replacement installed", install_fixture(argv[4], target));
    stnlabz_hotload_poll(watch);
    CHECK("qualified replacement becomes active", active_version(&registry, 2U));
    CHECK("replacement activation audit", log_contains(module_log, "replacement_active"));
    CHECK("replacement owns live shared object",
          stnlabz_module_loader_find(&loader, "fixture") != NULL &&
          stnlabz_module_loader_find(&loader, "fixture")->descriptor->version_patch == 2U);
    CHECK("no duplicate loader owners", loader.count == 1U);
    CHECK("no duplicate registry records", registry.count == 1U);
    CHECK("stop active fixture", stnlabz_core_stop(&registry, "fixture"));
    CHECK("stopped fixture state",
          stnlabz_module_registry_find(&registry, "fixture") &&
          stnlabz_module_registry_find(&registry, "fixture")->state ==
              STNLABZ_MODULE_STATE_STOPPED);

cleanup:
    if (watch) stnlabz_hotload_destroy(watch);
    if (initialized) {
        stnlabz_module_registry_destroy(&registry);
        stnlabz_module_loader_unload_all(&loader);
    }
    if (stnlabz_log_is_open()) stnlabz_log_close();
    (void)unlink(target);
    (void)unlink(core_log);
    (void)unlink(module_log);
    (void)rmdir(module_dir);
    (void)rmdir(modules);
    (void)rmdir(staging);
    (void)rmdir(base);
    printf("Core hotload integration: executed=%u passed=%u failed=%u\n",
           tests, passed, tests - passed);
    return tests >= 20U && tests == passed ? 0 : 1;
}
