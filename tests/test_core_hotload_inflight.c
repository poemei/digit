#define _POSIX_C_SOURCE 200809L
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "core_hotload.h"
#include "core_activation.h"
#include "module_host.h"
#include "logging.h"

typedef struct { atomic_int entered, release, completed; } fixture_block_t;
static fixture_block_t block_state;
static atomic_int hotload_completed;
static stnlabz_hotload_t *watch;
static const stnlabz_module_host_t *host;
static unsigned checks, passed;
#define CHECK(name, expr) do { int ok = !!(expr); ++checks; passed += (unsigned)ok; \
    printf("%s hotload-inflight %s\n", ok ? "PASS" : "FAIL", name); } while (0)

static int copy_atomic(const char *from, const char *target)
{
    char staged[PATH_MAX], data[16384];
    int fd = -1, out = -1, ok = 0;
    ssize_t n;
    if (snprintf(staged, sizeof staged, "%s.next.XXXXXX", target) >=
        (int)sizeof staged) return 0;
    fd = open(from, O_RDONLY);
    if (fd < 0) return 0;
    out = mkstemp(staged);
    if (out < 0) goto done;
    while ((n = read(fd, data, sizeof data)) != 0) {
        ssize_t off = 0;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) goto done;
        while (off < n) {
            ssize_t w = write(out, data + off, (size_t)(n - off));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) goto done;
            off += w;
        }
    }
    if (close(out)) { out = -1; goto done; }
    out = -1;
    if (rename(staged, target)) goto done;
    ok = 1;
done:
    if (fd >= 0) (void)close(fd);
    if (out >= 0) (void)close(out);
    if (!ok) (void)unlink(staged);
    return ok;
}
static int wait_atomic(atomic_int *flag, int value)
{
    struct timespec delay = {0, 1000000L};
    int i;
    for (i = 0; i < 3000; ++i) {
        if (atomic_load(flag) == value) return 1;
        (void)nanosleep(&delay, NULL);
    }
    return 0;
}
static void *call_service(void *unused)
{
    fixture_block_t *state = &block_state;
    (void)unused;
    return (void *)(long)host->invoke_service("fixture.block",
        &state, sizeof state, NULL, 0U, NULL);
}
static void *run_hotload(void *unused)
{
    (void)unused;
    stnlabz_hotload_poll(watch);
    atomic_store(&hotload_completed, 1);
    return NULL;
}
static int active_patch(const stnlabz_module_registry_t *registry, unsigned patch)
{
    const stnlabz_module_record_t *r = stnlabz_module_registry_find(registry, "fixture");
    return r && r->state == STNLABZ_MODULE_STATE_ACTIVE &&
        r->descriptor.version_patch == patch;
}
int main(int argc, char **argv)
{
    char dir[] = "/tmp/digit-inflight-XXXXXX";
    char modules[PATH_MAX], module_dir[PATH_MAX], target[PATH_MAX];
    char staging[PATH_MAX], corelog[PATH_MAX], modlog[PATH_MAX];
    stnlabz_module_registry_t registry;
    stnlabz_module_loader_t loader;
    pthread_t caller, replacer;
    void *invoke_result = NULL;
    int initialized = 0, caller_started = 0, replacer_started = 0;
    if (argc != 3) { fputs("usage: test_core_hotload_inflight blocking.so good.so\n", stderr); return 2; }
    if (!mkdtemp(dir)) return 2;
#define MAKE_PATH(dest,fmt) do { if (snprintf(dest,sizeof dest,fmt,dir) >= (int)sizeof dest) goto cleanup; } while (0)
    MAKE_PATH(modules, "%s/modules");
    MAKE_PATH(module_dir, "%s/modules/fixture");
    MAKE_PATH(target, "%s/modules/fixture/fixture.so");
    MAKE_PATH(staging, "%s/staging");
    MAKE_PATH(corelog, "%s/core.log");
    MAKE_PATH(modlog, "%s/modules.log");
    if (mkdir(modules,0700) || mkdir(module_dir,0700) || mkdir(staging,0700)) goto cleanup;
    if (setenv("DIGIT_HOTLOAD_TEST_STAGE",staging,1)) goto cleanup;
    if (!stnlabz_log_open_path(corelog)) goto cleanup;
    stnlabz_module_registry_init(&registry);
    stnlabz_module_loader_init(&loader);
    initialized = 1;
    host = digit_module_host();
    watch = stnlabz_hotload_create(modules,&registry,&loader,host);
    CHECK("watch created",watch != NULL);
    if (!watch) goto cleanup;
    CHECK("incumbent fixture installed",copy_atomic(argv[1],target));
    stnlabz_hotload_poll(watch);
    CHECK("incumbent ACTIVE",active_patch(&registry,0U));
    CHECK("replacement installed",copy_atomic(argv[2],target));
    if (pthread_create(&caller,NULL,call_service,NULL)) goto cleanup;
    caller_started = 1;
    CHECK("request entered blocking module",wait_atomic(&block_state.entered,1));
    if (!atomic_load(&block_state.entered)) goto cleanup;
    if (pthread_create(&replacer,NULL,run_hotload,NULL)) goto cleanup;
    replacer_started = 1;
    {
        struct timespec t = {0, 30000000L};
        (void)nanosleep(&t,NULL);
    }
    CHECK("replacement does not finish during in-flight request",
          atomic_load(&hotload_completed) == 0);
    CHECK("request still blocked",atomic_load(&block_state.completed) == 0);
    atomic_store(&block_state.release,1);
    (void)pthread_join(caller,&invoke_result);
    caller_started = 0;
    CHECK("in-flight request returns OK",(long)invoke_result == STNLABZ_MODULE_OK);
    CHECK("request callback completed",atomic_load(&block_state.completed) == 1);
    (void)pthread_join(replacer,NULL);
    replacer_started = 0;
    CHECK("replacement poll finished",atomic_load(&hotload_completed) == 1);
    CHECK("new module ACTIVE",active_patch(&registry,2U));
    CHECK("one loader owner",loader.count == 1U);
    CHECK("one registry entry",registry.count == 1U);
    CHECK("old route withdrawn",
          host->invoke_service("fixture.block",NULL,0,NULL,0,NULL) ==
            STNLABZ_MODULE_ERR_NOT_FOUND);
    CHECK("stop new module",stnlabz_core_stop(&registry,"fixture"));
cleanup:
    atomic_store(&block_state.release,1);
    if (caller_started) (void)pthread_join(caller,NULL);
    if (replacer_started) (void)pthread_join(replacer,NULL);
    if (watch) stnlabz_hotload_destroy(watch);
    if (initialized) {
        stnlabz_module_registry_destroy(&registry);
        stnlabz_module_loader_unload_all(&loader);
    }
    if (stnlabz_log_is_open()) stnlabz_log_close();
    (void)unlink(target); (void)unlink(corelog); (void)unlink(modlog);
    (void)rmdir(module_dir); (void)rmdir(modules); (void)rmdir(staging); (void)rmdir(dir);
    printf("Core inflight hotload: checks=%u passed=%u failed=%u\n",checks,passed,checks-passed);
    return checks == 13U && passed == checks ? 0 : 1;
}
