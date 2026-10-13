#ifndef DIGIT_ROUTER_H
#define DIGIT_ROUTER_H
#include "stnlabz_abi/module.h"
#define DIGIT_ROUTER_MAX_ROUTES 16
#define DIGIT_ROUTER_PATH_MAX 64
#define DIGIT_ROUTER_SERVICE_MAX STNLABZ_MODULE_SERVICE_NAME_MAX
#define DIGIT_ROUTER_REQUEST_ID_MAX 64
typedef enum {
 DIGIT_ROUTE_OK=0, DIGIT_ROUTE_DENIED, DIGIT_ROUTE_UNKNOWN,
 DIGIT_ROUTE_UNAVAILABLE, DIGIT_ROUTE_INVALID, DIGIT_ROUTE_FAILED
} digit_route_status_t;
typedef struct {
 char request_id[DIGIT_ROUTER_REQUEST_ID_MAX];
 char path[DIGIT_ROUTER_PATH_MAX];
 int core_authorized; /* Assertion from Core, NOT an authentication primitive. */
 const void *payload;
 size_t payload_size;
} digit_router_request_t;
typedef struct {
 digit_route_status_t status;
 char request_id[DIGIT_ROUTER_REQUEST_ID_MAX];
 char selected_service[DIGIT_ROUTER_SERVICE_MAX];
 stnlabz_module_result_t service_result;
 size_t response_used;
} digit_router_result_t;
/* Configuration by trusted Bootstrap prior to activation; no model execution. */
int digit_router_bind(const char *path,const char *service);
void digit_router_reset(void);
stnlabz_module_result_t digit_router_qualify(stnlabz_module_qualification_result_t *result);
stnlabz_module_result_t digit_router_start(const stnlabz_module_host_t *host);
stnlabz_module_result_t digit_router_stop(void);
stnlabz_module_descriptor_t digit_router_descriptor(void);
/* Called by admitted router service; response buffer passed unmodified to one model. */
digit_route_status_t digit_router_dispatch(const digit_router_request_t *request,
 void *response,size_t response_capacity,digit_router_result_t *result);
#endif
