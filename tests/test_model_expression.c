#include <stdio.h>
#include <string.h>
#include "model_expression.h"
#include "bootstrap.h"
#include "logging.h"
static unsigned checks,failures,lookups;
static stnlabz_module_service_handler_fn expression_handler,router_handler;
static void check(int x,const char *msg){++checks;if(!x){++failures;fprintf(stderr,"FAIL model expression: %s\n",msg);}}
static int register_service(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(!strcmp(name,"expression.lookup")){expression_handler=fn;return 1;}
 if(!strcmp(name,"router.dispatch")){router_handler=fn;return 1;}return 0;
}
static int unregister_service(const char *name,void *ctx){
 (void)ctx;if(!strcmp(name,"expression.lookup")){expression_handler=NULL;return 1;}
 if(!strcmp(name,"router.dispatch")){router_handler=NULL;return 1;}return 0;
}
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t sz,
 void *out,size_t cap,size_t *used){
 if(strcmp(name,"expression.lookup")||!expression_handler)return STNLABZ_MODULE_ERR_NOT_FOUND;
 ++lookups;return expression_handler(req,sz,out,cap,used,NULL);
}
int main(int argc,char **argv){
 stnlabz_module_registry_t reg;
 stnlabz_module_descriptor_t descriptor;
 stnlabz_module_host_t host={0};
 digit_bootstrap_t bootstrap={0};
 digit_policy_request_t policy={"operator","mission","module.activate","router",1,1,1,1,NULL,"test-fixture",1};
 digit_expression_request_t input={{0}};
 digit_expression_response_t output;
 digit_router_request_t route={"request-1","/lookup",1,&input,sizeof input};
 digit_router_result_t route_result;
 const stnlabz_module_record_t *record;
 unsigned before;size_t used=0;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 stnlabz_module_registry_init(&reg);
 host.register_service=register_service;host.unregister_service=unregister_service;host.invoke_service=invoke;
 descriptor=digit_expression_descriptor();
 check(!strcmp(descriptor.id,"expression")&&descriptor.required_core_api_minor==5,"Model ABI descriptor");
 check(stnlabz_module_registry_discover(&reg,&descriptor)==STNLABZ_MODULE_OK,"Model discover");
 check(stnlabz_module_registry_verify(&reg,"expression")==STNLABZ_MODULE_OK,"Model verify");
 check(stnlabz_module_registry_qualify(&reg,"expression")==STNLABZ_MODULE_OK,"Model qualification");
 check(stnlabz_module_registry_activate(&reg,"expression")==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"qualified Model not automatically active");
 check(digit_core_authorize(&reg,"expression",&policy)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"wrong target denied");
 policy.module_id="expression";
 check(digit_core_authorize(&reg,"expression",&policy)==STNLABZ_MODULE_OK,"Model Core authorized");
 check(digit_core_activate(&reg,"expression",&host,&policy)==STNLABZ_MODULE_OK,"Model Core activated");
 check(expression_handler!=NULL,"Model service registered");
 policy.module_id="router";
 check(digit_bootstrap_start(&bootstrap,&reg,&host,&policy,"/lookup","expression.lookup"),"Bootstrap admits Router");
 check(router_handler!=NULL,"Router registered");
 strcpy(input.expression,"purrs");
 check(digit_bootstrap_handoff(&bootstrap,&route,&output,sizeof output,&route_result)==DIGIT_ROUTE_OK &&
 output.status==DIGIT_EXPRESSION_KNOWN&&!strcmp(output.meaning,"cat vocalization") &&
 !strcmp(output.evidence,"fixture:expression-v1"),"known expression with explicit fixture provenance");
 check(output.source_line>0&&!strcmp(output.source_file,"src/model_expression.c"),"Model records own decision location");
 before=lookups;strcpy(input.expression,"puurs");
 check(digit_bootstrap_handoff(&bootstrap,&route,&output,sizeof output,&route_result)==DIGIT_ROUTE_OK &&
 output.status==DIGIT_EXPRESSION_UNKNOWN&&output.meaning[0]==0&&output.evidence[0]==0,
 "unknown expression not invented");
 check(lookups==before+1,"exactly one Model invocation");
 route.core_authorized=0;before=lookups;
 check(digit_bootstrap_handoff(&bootstrap,&route,&output,sizeof output,&route_result)==DIGIT_ROUTE_DENIED,"denial prevents model");
 check(lookups==before,"denied request invokes no Model");
 route.core_authorized=1;strcpy(route.path,"/unsupported");
 check(digit_bootstrap_handoff(&bootstrap,&route,&output,sizeof output,&route_result)==DIGIT_ROUTE_UNKNOWN,"unknown route");
 strcpy(route.path,"/lookup");memset(&input,0,sizeof input);
 check(digit_bootstrap_handoff(&bootstrap,&route,&output,sizeof output,&route_result)==DIGIT_ROUTE_OK &&
 output.status==DIGIT_EXPRESSION_INVALID,"invalid model input explicit");
 check(digit_bootstrap_stop(&bootstrap),"Bootstrap stops router");
 check(expression_handler!=NULL,"Model retained independently of Router");
 record=stnlabz_module_registry_find(&reg,"expression");
 check(record&&record->state==STNLABZ_MODULE_STATE_ACTIVE&&record->activation_authorized,
       "Bootstrap stops Router only; Model stays active and authorized");
 check(digit_core_stop_all(&reg),"Core stops Model");
 check(expression_handler==NULL,"Model unregisters service");
 record=stnlabz_module_registry_find(&reg,"expression");
 check(record&&record->state==STNLABZ_MODULE_STATE_STOPPED&&!record->activation_authorized,"Model stopped and grant revoked");
 check(digit_expression_lookup(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
 "stopped Model cannot be invoked");
 stnlabz_module_registry_destroy(&reg);digit_log_close();
 printf("Core model_expression tests: %u executed, %u failed\n",checks,failures);return failures!=0;
}
