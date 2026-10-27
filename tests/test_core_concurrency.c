#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "module_host.h"
#include "digit_route.h"

static atomic_int entered, release_handler, finished, unregister_done, withdrawal_started;
static atomic_int calls, starts;
static const stnlabz_module_host_t *host;
static int service_context, command_context;
static unsigned checks, passed;
#define CHECK(msg, expr) do { int ok = !!(expr); ++checks; passed += (unsigned)ok; \
    printf("%s concurrent %s\n", ok ? "PASS" : "FAIL", msg); } while (0)

static int wait_for(atomic_int *p, int desired)
{
    struct timespec sleep_time = {0, 1000000L};
    int i;
    for (i = 0; i < 3000; ++i) {
        if (atomic_load(p) == desired) return 1;
        (void)nanosleep(&sleep_time, NULL);
    }
    return 0;
}
static stnlabz_module_result_t service(
    const void *req, size_t req_len, void *response, size_t response_len,
    size_t *used, void *ctx)
{
    (void)req; (void)req_len; (void)response; (void)response_len;
    (void)used;
    if (ctx != &service_context) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    atomic_fetch_add(&calls, 1);
    atomic_store(&entered, 1);
    while (!atomic_load(&release_handler)) {
        struct timespec t = {0, 1000000L};
        (void)nanosleep(&t, NULL);
    }
    atomic_store(&finished, 1);
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t command(
    const stnlabz_module_command_t *cmd, stnlabz_module_command_reply_fn reply,
    void *reply_ctx, void *ctx)
{
    (void)cmd; (void)reply; (void)reply_ctx;
    if (ctx != &command_context) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    atomic_fetch_add(&calls, 1);
    atomic_store(&entered, 1);
    while (!atomic_load(&release_handler)) {
        struct timespec t = {0, 1000000L};
        (void)nanosleep(&t, NULL);
    }
    atomic_store(&finished, 1);
    return STNLABZ_MODULE_OK;
}
static void reset(void)
{
    atomic_store(&entered, 0);
    atomic_store(&release_handler, 0);
    atomic_store(&finished, 0);
    atomic_store(&unregister_done, 0);
    atomic_store(&withdrawal_started, 0);
    atomic_store(&calls, 0);
    atomic_store(&starts, 0);
}
static void *call_service(void *unused)
{
    (void)unused;
    atomic_store(&starts, 1);
    return (void *)(long)host->invoke_service("test.concurrent", NULL, 0, NULL, 0, NULL);
}
static void *call_command(void *unused)
{
    digit_route_request_t r;
    digit_route_reply_t out;
    (void)unused;
    memset(&r, 0, sizeof r);
    (void)snprintf(r.sender, sizeof r.sender, "tester");
    (void)snprintf(r.text, sizeof r.text, "/concurrent hello");
    atomic_store(&starts, 1);
    return (void *)(long)host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
        &r, sizeof r, &out, sizeof out, NULL);
}
static void *withdraw_service(void *unused)
{
    int ok;
    (void)unused;
    atomic_store(&withdrawal_started, 1);
    ok = host->unregister_service("test.concurrent", &service_context);
    atomic_store(&unregister_done, ok ? 1 : -1);
    return NULL;
}
static void *withdraw_command(void *unused)
{
    int ok;
    (void)unused;
    atomic_store(&withdrawal_started, 1);
    ok = host->unregister_command("concurrent", &command_context);
    atomic_store(&unregister_done, ok ? 1 : -1);
    return NULL;
}
static void run_case(int use_command)
{
    pthread_t worker, withdrawer;
    void *result = NULL;
    int ready;
    reset();
    CHECK("register handler",
        use_command ? host->register_command("concurrent", command, &command_context) :
        host->register_service("test.concurrent", service, &service_context));
    if (pthread_create(&worker, NULL, use_command ? call_command : call_service, NULL)) {
        CHECK("worker launched", 0); return;
    }
    ready = wait_for(&entered, 1);
    CHECK("handler entered before withdrawal", ready);
    if (!ready) {
        atomic_store(&release_handler, 1);
        (void)pthread_join(worker, NULL);
        return;
    }
    if (pthread_create(&withdrawer, NULL,
            use_command ? withdraw_command : withdraw_service, NULL)) {
        CHECK("withdrawal worker launched", 0);
        atomic_store(&release_handler, 1);
        (void)pthread_join(worker, NULL);
        return;
    }
    CHECK("withdrawal thread started", wait_for(&withdrawal_started, 1));
    /* Wait for the registration to be marked withdrawing. The caller must
       remain blocked until the in-flight handler completes. */
    {
        struct timespec t = {0, 20000000L};
        (void)nanosleep(&t, NULL);
    }
    CHECK("withdrawal waits for in-flight completion",
          atomic_load(&unregister_done) == 0);
    atomic_store(&release_handler, 1);
    (void)pthread_join(worker, &result);
    (void)pthread_join(withdrawer, NULL);
    CHECK("original handler completed", atomic_load(&finished) == 1);
    CHECK("original invocation returns OK", (long)result == STNLABZ_MODULE_OK);
    CHECK("withdrawal completes", atomic_load(&unregister_done) == 1);
    CHECK("handler invoked once", atomic_load(&calls) == 1);
    if (use_command) {
        digit_route_request_t r = {0};
        digit_route_reply_t out;
        (void)snprintf(r.text, sizeof r.text, "/concurrent again");
        CHECK("withdrawn command no longer callable",
            host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
                &r, sizeof r, &out, sizeof out, NULL) ==
                    STNLABZ_MODULE_ERR_NOT_FOUND);
        CHECK("command slot reusable",
            host->register_command("concurrent", command, &command_context));
        CHECK("command removable", host->unregister_command("concurrent", &command_context));
    } else {
        CHECK("withdrawn service no longer callable",
            host->invoke_service("test.concurrent", NULL, 0, NULL, 0, NULL) ==
                STNLABZ_MODULE_ERR_NOT_FOUND);
        CHECK("service slot reusable",
            host->register_service("test.concurrent", service, &service_context));
        CHECK("service removable", host->unregister_service("test.concurrent", &service_context));
    }
}
int main(void)
{
    host = digit_module_host();
    CHECK("host available", host != NULL);
    if (!host) return 1;
    run_case(0);
    run_case(1);
    printf("Core concurrent routing: checks=%u passed=%u failed=%u\n",
           checks, passed, checks - passed);
    return checks >= 23U && checks == passed ? 0 : 1;
}
