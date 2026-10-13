#include <stdio.h>
#include <string.h>
#include "router.h"
#include "stnlabz_abi/module_registry.h"
#include "logging.h"
static unsigned checks,failures,calls,registrations,unregistrations;
static stnlabz_module_service_handler_fn service_handler;
static int check(int ok,const char *what){++checks;if(!ok){++failures;fprintf(stderr,"FAIL router: %s\n",what);}return ok;}
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t req_size,
 void *resp,size_t cap,size_t *used){
 ++calls;(void)req;(void)req_size;
 if(strcmp(name,"mock.echo"))return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(!resp||cap<3||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(resp,"ok",3);*used=3;return STNLABZ_MODULE_OK;
}
static int register_service(const char *name,stnlabz_module_service_handler_fn handler,void *context){
 (void)context;++registrations;
 if(strcmp(name,"router.dispatch")||!handler)return 0;
 service_handler=handler;return 1;
}
static int unregister_service(const char *name,void *context){
 (void)context;++unregistrations;
 if(strcmp(name,"router.dispatch"))return 0;
 service_handler=NULL;return 1;
}
int main(int argc,char **argv){
 stnlabz_module_host_t host={0};
 stnlabz_module_descriptor_t descriptor;
 stnlabz_module_registry_t registry;
 stnlabz_module_qualification_result_t q;
 digit_router_request_t req={"req-1","/ask",1,"question",8};
 digit_router_result_t result;
 char output[16];size_t used=0;
 unsigned before;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 digit_router_reset();
 descriptor=digit_router_descriptor();
 stnlabz_module_registry_init(&registry);
 check(stnlabz_module_registry_discover(&registry,&descriptor)==STNLABZ_MODULE_OK,"Core ABI discovers router");
 check(stnlabz_module_registry_verify(&registry,"router")==STNLABZ_MODULE_OK,"Core ABI verifies router");
 check(stnlabz_module_registry_qualify(&registry,"router")==STNLABZ_MODULE_OK,"Core ABI qualifies router");
 check(!strcmp(descriptor.id,"router")&&descriptor.version_major==0&&descriptor.version_minor==1,"versioned ABI descriptor");
 check(descriptor.required_core_api_major==STNLABZ_MODULE_API_MAJOR&&
 descriptor.required_core_api_minor==STNLABZ_MODULE_API_MINOR,"ABI 1.5 match");
 check(descriptor.qualify(&q)==STNLABZ_MODULE_OK&&q.tests_executed>=STNLABZ_MODULE_MIN_TESTS,"qualification contract");
 check(digit_router_bind("/ask","mock.echo"),"bind route");
 check(!digit_router_bind("/ask","second"),"reject duplicate");
 check(!digit_router_bind("ask","mock.echo"),"reject malformed path");
 check(!digit_router_bind("/bad route","mock.echo"),"reject whitespace path");
 check(!digit_router_bind("/empty",""),"reject missing service");
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_UNAVAILABLE,"route unavailable before start");
 check(calls==0,"no invocation without host");
 host.invoke_service=invoke;host.register_service=register_service;host.unregister_service=unregister_service;
 check(descriptor.start(&host)==STNLABZ_MODULE_OK,"start Router via ABI");
 check(registrations==1&&service_handler!=NULL,"service registered");
 check(!digit_router_bind("/later","mock.echo"),"configuration frozen after start");
 req.core_authorized=0;
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_DENIED,"Core-denied execution blocked");
 check(calls==0,"denied request invokes no Model");
 req.core_authorized=1;strcpy(req.path,"/unknown");
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_UNKNOWN,"unknown route");
 check(calls==0,"unknown route invokes no Model");
 strcpy(req.path,"/ask");
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_OK&&
 !strcmp(result.selected_service,"mock.echo")&&result.response_used==3&&
 !strcmp(output,"ok"),"known route dispatches one service");
 check(calls==1,"exactly one Model invoked");
 before=calls;req.core_authorized=0;
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_DENIED,"revocation denied");
 check(calls==before,"revoked request invokes no Model");
 req.core_authorized=1;
 check(service_handler!=NULL,"ABI service exposed");
 if(service_handler){
 check(service_handler(&req,sizeof req,output,sizeof output,&used,NULL)==STNLABZ_MODULE_OK&&used==3,
 "ABI service delegates exactly once");
 }
 check(calls==before+1,"no extra chain");
 check(descriptor.stop()==STNLABZ_MODULE_OK,"stop Router");
 check(unregistrations==1&&service_handler==NULL,"service unregistered");
 check(digit_router_dispatch(&req,output,sizeof output,&result)==DIGIT_ROUTE_UNAVAILABLE,"stopped router unavailable");
 check(descriptor.stop()==STNLABZ_MODULE_ERR_INVALID_STATE,"duplicate stop rejected");
 digit_router_reset();stnlabz_module_registry_destroy(&registry);digit_log_close();
 printf("Core router tests: %u executed, %u failed\n",checks,failures);
 return failures!=0;
}
