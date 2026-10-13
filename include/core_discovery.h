#ifndef DIGIT_CORE_DISCOVERY_H
#define DIGIT_CORE_DISCOVERY_H
#include "stnlabz_abi/module_discovery.h"
/* Discovery only: never verifies, qualifies, authorizes or activates. */
int digit_core_discover(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader, const char *path,
    stnlabz_module_discovery_report_t *report);
void digit_core_discovery_cleanup(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader);
#endif
