#include <string.h>
#include "bootstrap.h"
#include "logging.h"
static int record(const char *event,const char *status){
 return digit_log_event("BOOTSTRAP",event,status)&&digit_log_sync();
}
int digit_bootstrap_start(digit_bootstrap_t *b,stnlabz_module_registry_t *registry,
 const stnlabz_module_host_t *host,const digit_policy_request_t *policy,
 const char *path,const char *service){
 stnlabz_module_descriptor_t d;
 const stnlabz_module_record_t *existing;
 if(!b||!registry||b->ready||!digit_log_is_open())return 0;
 b->registry=NULL;b->ready=0;
 if(!policy||!host||!path||!service||
 !policy->operation||!policy->module_id||
 strcmp(policy->operation,"module.activate")||
 strcmp(policy->module_id,"router")||
 !policy->context_verified)return 0;
 existing=stnlabz_module_registry_find(registry,"router");
 if(existing)return 0; /* No implicit adoption or reactivation. */
 digit_router_reset();
 if(!digit_router_bind(path,service))return 0;
 d=digit_router_descriptor();
 if(stnlabz_module_registry_discover(registry,&d)!=STNLABZ_MODULE_OK)goto fail;
 if(stnlabz_module_registry_verify(registry,"router")!=STNLABZ_MODULE_OK)goto fail;
 if(stnlabz_module_registry_qualify(registry,"router")!=STNLABZ_MODULE_OK)goto fail;
 if(digit_core_authorize(registry,"router",policy)!=STNLABZ_MODULE_OK)goto fail;
 if(digit_core_activate(registry,"router",host,policy)!=STNLABZ_MODULE_OK)goto fail;
 if(!record("READY","status=router_active"))goto fail;
 b->registry=registry;b->ready=1;return 1;
fail:
 /* Stop on partial activation before dropping the registry entry. */
 (void)digit_core_stop_all(registry);
 (void)stnlabz_module_registry_unregister(registry,"router");
 digit_router_reset();
 (void)record("FAILED","status=start_rejected");
 return 0;
}
digit_route_status_t digit_bootstrap_handoff(const digit_bootstrap_t *b,
 const digit_router_request_t *request,void *response,size_t capacity,
 digit_router_result_t *result){
 const stnlabz_module_record_t *router;
 if(!b||!b->ready||!b->registry||!result)return DIGIT_ROUTE_UNAVAILABLE;
 router=stnlabz_module_registry_find(b->registry,"router");
 if(!router||router->state!=STNLABZ_MODULE_STATE_ACTIVE)return DIGIT_ROUTE_UNAVAILABLE;
 /* Bootstrap is not a controller. Dispatch remains Router-owned. */
 return digit_router_dispatch(request,response,capacity,result);
}
int digit_bootstrap_stop(digit_bootstrap_t *b){
 const stnlabz_module_record_t *router;
 if(!b||!b->ready||!b->registry||!digit_log_is_open())return 0;
 router=stnlabz_module_registry_find(b->registry,"router");
 if(!router||router->state!=STNLABZ_MODULE_STATE_ACTIVE)return 0;
 if(!digit_core_stop_all(b->registry))return 0;
 if(!record("STOPPED","status=router_stopped"))return 0;
 b->ready=0;b->registry=NULL;digit_router_reset();return 1;
}
