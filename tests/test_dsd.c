#include <stdio.h>
#include <string.h>
#include "dsd.h"
#include "core_activation.h"
#include "logging.h"
static unsigned checks,failed,registered,unregistered;
static stnlabz_module_service_handler_fn handler;
static void test(int ok,const char *why){++checks;if(!ok){++failed;fprintf(stderr,"FAIL dsd: %s\n",why);}}
static int reg(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(name,"dsd.report"))return 0;handler=fn;++registered;return 1;
}
static int unreg(const char *name,void *ctx){
 (void)ctx;if(strcmp(name,"dsd.report"))return 0;handler=NULL;++unregistered;return 1;
}
int main(int argc,char **argv){
 stnlabz_module_registry_t registry;stnlabz_module_descriptor_t d;
 stnlabz_module_host_t host={0};
 digit_policy_request_t policy={"operator","test","module.activate","dsd",1,1,1,1,NULL,"trusted-fixture",1};
 digit_dsd_trace_t trace={0};
 digit_dsd_report_t report;
 size_t used=0;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 stnlabz_module_registry_init(&registry);
 host.register_service=reg;host.unregister_service=unreg;
 d=digit_dsd_descriptor();
 test(!strcmp(d.id,"dsd")&&d.version_minor==1&&d.required_core_api_minor==5,"ABI 1.5 descriptor");
 test(stnlabz_module_registry_discover(&registry,&d)==STNLABZ_MODULE_OK,"discovery");
 test(stnlabz_module_registry_verify(&registry,"dsd")==STNLABZ_MODULE_OK,"verification");
 test(stnlabz_module_registry_qualify(&registry,"dsd")==STNLABZ_MODULE_OK,"qualification");
 test(stnlabz_module_registry_activate(&registry,"dsd")==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"cannot autoactivate");
 test(digit_core_authorize(&registry,"dsd",&policy)==STNLABZ_MODULE_OK,"Core authorization");
 test(digit_core_activate(&registry,"dsd",&host,&policy)==STNLABZ_MODULE_OK,"Core activation");
 test(handler!=NULL&&registered==1,"service published");
 strcpy(trace.request_id,"request-9");trace.count=2;
 strcpy(trace.observations[0].component,"ROUTER");
 strcpy(trace.observations[0].decision,"ROUTED");
 strcpy(trace.observations[0].reason,"selected expression.lookup");
 strcpy(trace.observations[1].component,"EXPRESSION");
 strcpy(trace.observations[1].decision,"FAILED");
 strcpy(trace.observations[1].file,"src/model_expression.c");
 strcpy(trace.observations[1].function,"digit_expression_lookup");
 trace.observations[1].line=42;
 strcpy(trace.observations[1].reason,"fixture_failure");
 test(handler(&trace,sizeof trace,&report,sizeof report,&used,NULL)==STNLABZ_MODULE_OK,"report available");
 test(used==sizeof report&&report.has_failure&&report.observations==2,"failure marked");
 test(strstr(report.report,"file=src/model_expression.c function=digit_expression_lookup line=42")!=NULL,
 "reported actual supplied location, no guess");
 test(strstr(report.report,"file=not_recorded function=not_recorded line=0")!=NULL,
 "missing location explicitly unrecorded");
 test(!strcmp(report.request_id,"request-9"),"request correlation");
 trace.count=DIGIT_DSD_TRACE_MAX+1;
 test(handler(&trace,sizeof trace,&report,sizeof report,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
 "bounded observations");
 trace.count=2;memset(trace.observations[1].reason,'x',sizeof trace.observations[1].reason);
 test(handler(&trace,sizeof trace,&report,sizeof report,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
 "reject unterminated evidence");
 test(digit_core_stop(&registry,"dsd"),"Core stops DSD only");
 test(handler==NULL&&unregistered==1,"service withdrawn");
 test(digit_dsd_report(&trace,sizeof trace,&report,sizeof report,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
 "inactive DSD refuses execution");
 stnlabz_module_registry_destroy(&registry);digit_log_close();
 printf("Core dsd tests: %u executed, %u failed\n",checks,failed);
 return failed!=0;
}
