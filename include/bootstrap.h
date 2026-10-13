#ifndef DIGIT_BOOTSTRAP_H
#define DIGIT_BOOTSTRAP_H
#include "core_activation.h"
#include "router.h"
typedef struct {
 stnlabz_module_registry_t *registry;
 int ready;
} digit_bootstrap_t;
/* Trusted caller supplies Core policy context; Bootstrap cannot create authority. */
int digit_bootstrap_start(digit_bootstrap_t *bootstrap,
 stnlabz_module_registry_t *registry,const stnlabz_module_host_t *host,
 const digit_policy_request_t *policy,const char *path,const char *service);
/* Handoff only. Router retains sole responsibility for route selection. */
digit_route_status_t digit_bootstrap_handoff(const digit_bootstrap_t *bootstrap,
 const digit_router_request_t *request,void *response,size_t capacity,
 digit_router_result_t *result);
int digit_bootstrap_stop(digit_bootstrap_t *bootstrap);
#endif
