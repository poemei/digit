#include <stdio.h>
#include <string.h>
#include "mvc_console.h"
#include "core_activation.h"
#include "logging.h"
#include "bootstrap.h"
#include "model_expression.h"
#include "rendering.h"
#include "dsd.h"
#include "stnlabz_abi/module_loader.h"
#include "stnlabz_abi/module_discovery.h"

typedef struct {const char *name;stnlabz_module_service_handler_fn handler;void *context;} service_t;
static service_t services[8];
static size_t service_count;
static int add_service(const char *name,stnlabz_module_service_handler_fn handler,void *context){
 size_t i;
 if(!name||!handler||service_count>=8)return 0;
 for(i=0;i<service_count;i++)if(!strcmp(services[i].name,name))return 0;
 services[service_count].name=name;services[service_count].handler=handler;
 services[service_count++].context=context;return 1;
}
static int remove_service(const char *name,void *context){
 size_t i;
 for(i=0;i<service_count;i++)if(!strcmp(services[i].name,name)&&services[i].context==context){
  for(;i+1<service_count;i++)services[i]=services[i+1];
  --service_count;return 1;
 }
 return 0;
}
static stnlabz_module_result_t call_service(const char *name,const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used){
 size_t i;
 if(!name||!response_used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<service_count;i++)if(!strcmp(services[i].name,name))
  return services[i].handler(request,request_size,response,response_size,response_used,services[i].context);
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static const stnlabz_module_host_t host={
 NULL,NULL,NULL,NULL,add_service,remove_service,call_service
};
static int activate(stnlabz_module_registry_t *reg,stnlabz_module_descriptor_t descriptor,
 digit_policy_request_t *policy){
 if(stnlabz_module_registry_discover(reg,&descriptor)!=STNLABZ_MODULE_OK||
    stnlabz_module_registry_verify(reg,descriptor.id)!=STNLABZ_MODULE_OK||
    stnlabz_module_registry_qualify(reg,descriptor.id)!=STNLABZ_MODULE_OK)return 0;
 policy->module_id=descriptor.id;
 return digit_core_authorize(reg,descriptor.id,policy)==STNLABZ_MODULE_OK &&
 digit_core_activate(reg,descriptor.id,&host,policy)==STNLABZ_MODULE_OK;
}
int digit_mvc_console(void){
 stnlabz_module_registry_t registry;
 digit_bootstrap_t bootstrap={0};
 stnlabz_module_loader_t loader;
 const stnlabz_module_descriptor_t *dsd_descriptor=NULL;
 char module_dir[1024],module_file[1200];
 int dsd_ready=0;
 digit_policy_request_t policy={"local-console","mvc-development","module.activate",NULL,
  1,1,1,1,NULL,"explicit-local-cli-development-mode",1};
 digit_expression_request_t expression;
 digit_expression_response_t model;
 digit_render_request_t view_input;
 digit_render_response_t rendered;
 digit_dsd_trace_t trace={0};
 digit_dsd_report_t diagnostic;
 digit_router_result_t diagnostic_route;
 char diagnostic_view[2048];
 digit_router_request_t request;
 char line[256];
 size_t seq=0,used;
 int ok=1;
 stnlabz_module_registry_init(&registry);stnlabz_module_loader_init(&loader);service_count=0;
 if(!digit_log_open()){fputs("MVC: mandatory log unavailable\n",stderr);return 1;}
 if(!digit_log_event("MVC","START","mode=explicit_local_development authentication=not_provided")||
 !digit_log_sync()){ok=0;goto done;}
 /* Local opt-in development proof only. This fixture context is NOT an identity validator. */
 if(!activate(&registry,digit_expression_descriptor(),&policy)||
 !activate(&registry,digit_rendering_descriptor(),&policy)){ok=0;goto done;}
 /* The existing ABI loader obtains DSD exclusively from installed module files. */
 if(stnlabz_module_discovery_get_path(module_dir,sizeof module_dir)==STNLABZ_MODULE_OK &&
    snprintf(module_file,sizeof module_file,"%s/dsd/bin/dsd.so",module_dir)>0 &&
    strlen(module_file)<sizeof module_file &&
    stnlabz_module_loader_load(&loader,"dsd",module_file,&dsd_descriptor)==STNLABZ_MODULE_LOADER_OK &&
    dsd_descriptor &&
    stnlabz_module_registry_discover(&registry,dsd_descriptor)==STNLABZ_MODULE_OK &&
    stnlabz_module_registry_verify(&registry,"dsd")==STNLABZ_MODULE_OK &&
    stnlabz_module_registry_qualify(&registry,"dsd")==STNLABZ_MODULE_OK){
  policy.module_id="dsd";
  dsd_ready=digit_core_authorize(&registry,"dsd",&policy)==STNLABZ_MODULE_OK &&
    digit_core_activate(&registry,"dsd",&host,&policy)==STNLABZ_MODULE_OK;
 }
 policy.module_id="router";
 if(!digit_bootstrap_start_with_diagnostics(&bootstrap,&registry,&host,&policy,
 "/lookup","expression.lookup",dsd_ready?"dsd.report":NULL)){
  ok=0;goto done;
 }
 puts("STN-LABZ Digit | MVC local development console");
 puts("Modules ACTIVE: router, expression, rendering (local development mode)");
 puts(dsd_ready?"DSD ACTIVE: installed independent dsd.so":"DSD UNAVAILABLE: module not installed or not activated");
 puts("Enter an expression; /diagnose for last request; /quit to stop. This console does not authenticate users.");
 fflush(stdout);
 while(fgets(line,sizeof line,stdin)){
  size_t len=strlen(line);
  if(len&&!strchr(line,'\n')&&!feof(stdin)){
   int c;while((c=getchar())!='\n'&&c!=EOF){}
   puts("Invalid input: too long.");continue;
  }
  while(len&&(line[len-1]=='\n'||line[len-1]=='\r'))line[--len]=0;
  if(!strcmp(line,"/quit"))break;
  if(!strncmp(line,"/diagnose",9)||!strncmp(line,"diagnose ",9)){
   if(!dsd_ready){puts("DSD unavailable.");continue;}
   if(!trace.request_id[0]){puts("No previous request available.");continue;}
   memset(&request,0,sizeof request);
   (void)snprintf(request.request_id,sizeof request.request_id,"console-%zu",++seq);
   (void)snprintf(request.path,sizeof request.path,"/diagnose");
   request.core_authorized=1;
   request.payload=&trace;request.payload_size=sizeof trace;
   if(digit_bootstrap_handoff(&bootstrap,&request,&diagnostic,sizeof diagnostic,&diagnostic_route)!=DIGIT_ROUTE_OK ||
      digit_rendering_diagnostic(&diagnostic,diagnostic_view,sizeof diagnostic_view)!=STNLABZ_MODULE_OK){
    puts("Diagnostic request failed.");continue;
   }
   fputs(diagnostic_view,stdout);fflush(stdout);continue;
  }
  memset(&expression,0,sizeof expression);
  if(len>=sizeof expression.expression){puts("Invalid expression.");continue;}
  memcpy(expression.expression,line,len+1);
  memset(&request,0,sizeof request);
  (void)snprintf(request.request_id,sizeof request.request_id,"console-%zu",++seq);
  (void)snprintf(request.path,sizeof request.path,"/lookup");
  request.core_authorized=1; /* Trusted in-process development adapter only. */
  request.payload=&expression;request.payload_size=sizeof expression;
  memset(&model,0,sizeof model);memset(&view_input,0,sizeof view_input);
  view_input.route.status=digit_bootstrap_handoff(&bootstrap,&request,&model,sizeof model,&view_input.route);
  if(view_input.route.status==DIGIT_ROUTE_OK){
   view_input.model=model;view_input.model_present=1;
  }
  if(call_service("rendering.render",&view_input,sizeof view_input,
   &rendered,sizeof rendered,&used)!=STNLABZ_MODULE_OK){ok=0;break;}
  memset(&trace,0,sizeof trace);
  (void)snprintf(trace.request_id,sizeof trace.request_id,"%s",request.request_id);
  trace.count=2;
  (void)snprintf(trace.observations[0].component,sizeof trace.observations[0].component,"ROUTER");
  (void)snprintf(trace.observations[0].decision,sizeof trace.observations[0].decision,"%s",
   view_input.route.status==DIGIT_ROUTE_OK?"ROUTED":"FAILED");
  (void)snprintf(trace.observations[0].reason,sizeof trace.observations[0].reason,"route_status=%d",(int)view_input.route.status);
  if(view_input.route.status==DIGIT_ROUTE_OK){
   (void)snprintf(trace.observations[1].component,sizeof trace.observations[1].component,"EXPRESSION");
   (void)snprintf(trace.observations[1].decision,sizeof trace.observations[1].decision,"%s",
    model.status==DIGIT_EXPRESSION_INVALID?"FAILED":model.status==DIGIT_EXPRESSION_UNKNOWN?"UNKNOWN":"KNOWN");
   (void)snprintf(trace.observations[1].reason,sizeof trace.observations[1].reason,"model_status=%d",(int)model.status);
   (void)snprintf(trace.observations[1].file,sizeof trace.observations[1].file,"%s",model.source_file);
   (void)snprintf(trace.observations[1].function,sizeof trace.observations[1].function,"%s",model.source_function);
   trace.observations[1].line=model.source_line;
  }else trace.count=1;
  printf("%s\n",rendered.text);fflush(stdout);
 }
done:
 if(bootstrap.ready&&!digit_bootstrap_stop(&bootstrap))ok=0;
 if(!digit_core_stop_all(&registry))ok=0;
 if(!digit_log_event("MVC",ok?"STOPPED":"FAILED","mode=explicit_local_development")||
 !digit_log_sync())ok=0;
 if(registry.count==0)stnlabz_module_loader_unload_all(&loader);
 else digit_log_event("MVC","LOADER_RETAINED","reason=active_or_failed_module");
 stnlabz_module_registry_destroy(&registry);digit_log_close();
 if(!ok)fputs("MVC local console failed; inspect logs/digit.log\n",stderr);
 return ok?0:1;
}
