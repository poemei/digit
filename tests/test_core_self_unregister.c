#include <stdio.h>
#include <string.h>
#include "module_host.h"
#include "digit_route.h"

static const stnlabz_module_host_t *host;
static int service_context, command_context;
static int rejected_service, rejected_command;
static unsigned checks, passed;
#define CHECK(label, expr) do { int ok = !!(expr); ++checks; passed += (unsigned)ok; \
    printf("%s reentrant %s\n", ok ? "PASS" : "FAIL", label); } while (0)

static stnlabz_module_result_t self_service(
    const void *request, size_t request_size,
    void *response, size_t response_size, size_t *response_used, void *context)
{
    (void)request; (void)request_size; (void)response;
    (void)response_size; (void)response_used;
    rejected_service = !host->unregister_service("self.service", context);
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t self_command(
    const stnlabz_module_command_t *request,
    stnlabz_module_command_reply_fn reply, void *reply_context, void *context)
{
    (void)request; (void)reply; (void)reply_context;
    rejected_command = !host->unregister_command("self", context);
    return STNLABZ_MODULE_OK;
}
int main(void)
{
    digit_route_request_t req = {0};
    digit_route_reply_t response;
    host = digit_module_host();
    CHECK("Core host available", host != NULL);
    if (!host) return 1;
    CHECK("service registration", host->register_service(
        "self.service", self_service, &service_context));
    CHECK("self-unregister service invocation returns",
        host->invoke_service("self.service", NULL, 0, NULL, 0, NULL) ==
            STNLABZ_MODULE_OK);
    CHECK("self-unregister service rejected", rejected_service);
    CHECK("service registration remains active after rejection",
        host->invoke_service("self.service", NULL, 0, NULL, 0, NULL) ==
            STNLABZ_MODULE_OK);
    CHECK("external service withdrawal works",
        host->unregister_service("self.service", &service_context));
    CHECK("withdrawn service no longer callable",
        host->invoke_service("self.service", NULL, 0, NULL, 0, NULL) ==
            STNLABZ_MODULE_ERR_NOT_FOUND);

    CHECK("command registration", host->register_command(
        "self", self_command, &command_context));
    (void)snprintf(req.text, sizeof req.text, "/self test");
    CHECK("self-unregister command invocation returns",
        host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
            &req, sizeof req, &response, sizeof response, NULL) ==
                STNLABZ_MODULE_OK);
    CHECK("self-unregister command rejected", rejected_command);
    CHECK("command registration remains active after rejection",
        host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
            &req, sizeof req, &response, sizeof response, NULL) ==
                STNLABZ_MODULE_OK);
    CHECK("external command withdrawal works",
        host->unregister_command("self", &command_context));
    CHECK("withdrawn command no longer callable",
        host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
            &req, sizeof req, &response, sizeof response, NULL) ==
                STNLABZ_MODULE_ERR_NOT_FOUND);
    printf("Core reentrant withdrawal: checks=%u passed=%u failed=%u\n",
           checks, passed, checks - passed);
    return checks == 12U && checks == passed ? 0 : 1;
}
