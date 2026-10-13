#include <stdio.h>
#include <string.h>
#include "core_discovery.h"
#include "logging.h"

static int audit(const char *event, const char *detail)
{
    return digit_log_event("DISCOVERY", event, detail) && digit_log_sync();
}

int digit_core_discover(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader, const char *path,
    stnlabz_module_discovery_report_t *report)
{
    stnlabz_module_result_t rc;
    char detail[512];
    if (!registry || !loader || !path || !report || !digit_log_is_open())
        return 0;
    if (!audit("SCAN_START", "status=initiated")) return 0;
    rc = stnlabz_module_discovery_scan(registry, loader, path, report);
    if (rc == STNLABZ_MODULE_ERR_NOT_FOUND) {
        return audit("SCAN_ABSENT", "status=no_module_directory");
    }
    if (rc != STNLABZ_MODULE_OK) {
        (void)snprintf(detail, sizeof detail, "reason=%s",
                       stnlabz_module_result_string(rc));
        return audit("SCAN_FAILED", detail);
    }
    (void)snprintf(detail, sizeof detail,
        "examined=%zu loaded=%zu discovered=%zu rejected=%zu",
        report->directories_examined, report->modules_loaded,
        report->modules_discovered, report->modules_rejected);
    if (!audit("SCAN_COMPLETE", detail)) return 0;
    if (report->modules_rejected) {
        (void)snprintf(detail, sizeof detail,
            "module=%.63s stage=%.31s reason=%.127s rejected=%zu",
            report->rejected_module, report->rejection_stage,
            report->rejection_reason, report->modules_rejected);
        if (!audit("REJECTED_LAST", detail)) return 0;
    }
    /* Discovery never confers qualification or activation authority. */
    return 1;
}

void digit_core_discovery_cleanup(stnlabz_module_registry_t *registry,
    stnlabz_module_loader_t *loader)
{
    if (!registry || !loader) return;
    /* Unload only never-activated discovered candidates. */
    while (registry->count) {
        stnlabz_module_record_t *r = &registry->modules[registry->count - 1];
        if (r->state != STNLABZ_MODULE_STATE_DISCOVERED) break;
        (void)stnlabz_module_registry_fail(registry, r->descriptor.id);
        (void)stnlabz_module_registry_unregister(registry, r->descriptor.id);
    }
    if (registry->count == 0) stnlabz_module_loader_unload_all(loader);
}
