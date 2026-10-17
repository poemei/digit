#ifndef STNLABZ_MODULE_ABI_H
#define STNLABZ_MODULE_ABI_H

/*
 * STN-LABZ
 * Module ABI 1.5
 *
 * abi.h
 *
 * Reusable module lifecycle orchestration.
 */

#include "module.h"
#include "module_registry.h"

stnlabz_module_result_t
stnlabz_module_abi_prepare(
    stnlabz_module_registry_t *registry,
    const stnlabz_module_descriptor_t *descriptor
);

stnlabz_module_result_t
stnlabz_module_abi_authorize_and_activate(
    stnlabz_module_registry_t *registry,
    const char *module_id,
    const stnlabz_module_host_t *host
);

stnlabz_module_result_t
stnlabz_module_abi_stop(
    stnlabz_module_registry_t *registry,
    const char *module_id
);

stnlabz_module_result_t
stnlabz_module_abi_unregister(
    stnlabz_module_registry_t *registry,
    const char *module_id
);

stnlabz_module_result_t
stnlabz_module_abi_prepare_replacement(
    stnlabz_module_registry_t *registry,
    const char *module_id
);

/* Generic, versioned lifecycle notification. Independent transports register
 * "notify.<module-id>" using the existing host service callbacks. */
#define STNLABZ_NOTIFICATION_VERSION 1U
#define STNLABZ_NOTIFICATION_MESSAGE_MAX 4096
typedef struct {
    unsigned int version;
    char message[STNLABZ_NOTIFICATION_MESSAGE_MAX];
} stnlabz_module_notification_t;
typedef struct {
    unsigned int eligible, attempted, delivered, failed;
} stnlabz_notification_dispatch_result_t;

stnlabz_module_result_t stnlabz_module_abi_notify(
    const stnlabz_module_registry_t *registry,
    const stnlabz_module_host_t *host,
    const stnlabz_module_notification_t *notification,
    stnlabz_notification_dispatch_result_t *result
);

int
stnlabz_module_abi_self_test(void);

#endif
