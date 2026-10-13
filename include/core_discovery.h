#ifndef DIGIT_CORE_DISCOVERY_H
#define DIGIT_CORE_DISCOVERY_H
#include "stnlabz_abi/module_discovery.h"
/* Discovery and descriptor verification only; never qualifies or activates. */
int digit_core_discover(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader, const char *path,
    stnlabz_module_discovery_report_t *report);
/* Verify each DISCOVERED descriptor using unmodified ABI 1.5 lifecycle.
 * Failed candidates are isolated; audit failures fail the Core operation. */
int digit_core_verify(stnlabz_module_registry_t *registry);
void digit_core_discovery_cleanup(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader);
#endif
