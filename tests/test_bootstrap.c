#include <stdio.h>
#include <string.h>
#include "bootstrap.h"
#include "logging.h"
static unsigned assertions,failures,calls,registered,unregistered;
static stnlabz_module_service_handler_fn router_service;
static void check(int ok,const char *msg){++assertions;if(!ok){++failures;fprintf(stderr,"FAIL bootstrap: %s\n",msg);}}
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t sz,void *out,size_t cap,size_t *used){
 (void)req;(void)sz;++calls;if(strcmp(name,"fixture.reply"))return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(!out||cap<3)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(out,"ok",3);*used=3;return STNLABZ_MODULE_OK;
}
static int regservice(const char *name,stnlabz_module_service_handler_fn handler,void *ctx){
 (void)ctx;++registered;if(strcmp(name,"router.dispatch"))return 0;router_service=handler;return 1;
}
static int unregservice(const char *name,void *ctx){
 (void)ctx;++unregistered;if(strcmp(name,"router.dispatch"))return 0;router_service=NULL;return 1;
}
int main(int argc,char **argv){
 digit_bootstrap_t b={0};stnlabz_module_registry_t modules;
 stnlabz_module_host_t host={0};
 digit_policy_request_t p={"operator","mission","module.activate","router",1,1,1,1,NULL,"trusted-test-fixture",1};
 digit_router_request_t req={"request-1","/ask",1,"hi",3};
 digit_router_result_t result;
 char output[8]={0};
 const stnlabz_module_record_t *record;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 stnlabz_module_registry_init(&modules);
 host.invoke_service=invoke;host.register_service=regservice;host.unregister_service=unregservice;
 p.context_verified=0;
 check(!digit_bootstrap_start(&b,&modules,&host,&p,"/ask","fixture.reply"),"reject unverified start");
 check(modules.count==0,"no module admitted for unverified caller");
 p.context_verified=1;p.authority_granted=0;
 check(!digit_bootstrap_start(&b,&modules,&host,&p,"/ask","fixture.reply"),"reject unapproved authority");
 check(calls==0&&registered==0,"denied bootstrap invokes no service or startup");
 p.authority_granted=1;
 check(digit_bootstrap_start(&b,&modules,&host,&p,"/ask","fixture.reply"),"authorized Core bootstrap");
 record=stnlabz_module_registry_find(&modules,"router");
 check(record&&record->state==STNLABZ_MODULE_STATE_ACTIVE,"router admitted and active");
 check(registered==1&&router_service!=NULL,"router service registered");
 check(!digit_bootstrap_start(&b,&modules,&host,&p,"/ask","fixture.reply"),"duplicate bootstrap rejected");
 req.core_authorized=0;
 check(digit_bootstrap_handoff(&b,&req,output,sizeof output,&result)==DIGIT_ROUTE_DENIED,"route denial propagates");
 check(calls==0,"denied route invokes no model");
 req.core_authorized=1;
 check(digit_bootstrap_handoff(&b,&req,output,sizeof output,&result)==DIGIT_ROUTE_OK&&
 !strcmp(output,"ok")&&!strcmp(result.selected_service,"fixture.reply"),"handoff to single mock model");
 check(calls==1,"one callback per permitted request");
 strcpy(req.path,"/missing");
 check(digit_bootstrap_handoff(&b,&req,output,sizeof output,&result)==DIGIT_ROUTE_UNKNOWN,"unknown route explicit");
 check(calls==1,"unknown request no extra invocation");
 check(digit_bootstrap_stop(&b),"stop via Core module lifecycle");
 record=stnlabz_module_registry_find(&modules,"router");
 check(record&&record->state==STNLABZ_MODULE_STATE_STOPPED&&!record->activation_authorized,"stop revokes module authority");
 check(unregistered==1&&router_service==NULL,"stopping unregisters service");
 check(digit_bootstrap_handoff(&b,&req,output,sizeof output,&result)==DIGIT_ROUTE_UNAVAILABLE,"stopped handoff unavailable");
 check(!digit_bootstrap_stop(&b),"duplicate stop fails closed");
 stnlabz_module_registry_destroy(&modules);digit_log_close();
 printf("Core bootstrap tests: %u executed, %u failed\n",assertions,failures);
 return failures!=0;
}
