#ifndef DIGIT_CORE_HOTLOAD_H
#define DIGIT_CORE_HOTLOAD_H
#include "core_policy.h"
#include "stnlabz_abi/module_loader.h"
#include "stnlabz_abi/module_registry.h"
typedef struct digit_hotload digit_hotload_t;
digit_hotload_t *digit_hotload_create(const char *directory,
 stnlabz_module_registry_t *registry,stnlabz_module_loader_t *loader,
 const stnlabz_module_host_t *host,const digit_policy_request_t *policy);
void digit_hotload_poll(digit_hotload_t *watch);
void digit_hotload_destroy(digit_hotload_t *watch);
#endif
