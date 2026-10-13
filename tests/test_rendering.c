#include <stdio.h>
#include <string.h>
#include "rendering.h"
#include "bootstrap.h"
#include "logging.h"
static unsigned checks,failures,lookups;
static stnlabz_module_service_handler_fn model_fn,router_fn,view_fn;
static void check(int good,const char *name){++checks;if(!good){++failures;fprintf(stderr,"FAIL rendering: %s\n",name);}}
static int registration(const char *name,stnlabz_module_service_handler_fn fn,void *context){
 (void)context;
 if(!strcmp(name,"expression.lookup"))model_fn=fn;
 else if(!strcmp(name,"router.dispatch"))router_fn=fn;
 else if(!strcmp(name,"rendering.render"))view_fn=fn;
 else return 0;
 return 1;
}
static int unregistration(const char *name,void *context){
 (void)context;
 if(!strcmp(name,"expression.lookup"))model_fn=NULL;
 else if(!strcmp(name,"router.dispatch"))router_fn=NULL;
 else if(!strcmp(name,"rendering.render"))view_fn=NULL;
 else return 0;
 return 1;
}
static stnlabz_module_result_t invoke(const char *name,const void *request,size_t size,
 void *response,size_t capacity,size_t *used){
 if(!strcmp(name,"expression.lookup")&&model_fn){
  ++lookups;return model_fn(request,size,response,capacity,used,NULL);
 }
 if(!strcmp(name,"rendering.render")&&view_fn)
  return view_fn(request,size,response,capacity,used,NULL);
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
int main(int argc,char **argv){
 stnlabz_module_registry_t reg;
 stnlabz_module_host_t host={0};
 stnlabz_module_descriptor_t model,view;
 digit_bootstrap_t boot={0};
 digit_policy_request_t p={"operator","mission","module.activate","expression",1,1,1,1,NULL,"fixture",1};
 digit_expression_request_t expression={{0}};
 digit_router_request_t request={"mvc-1","/lookup",1,&expression,sizeof expression};
 digit_expression_response_t model_result;
 digit_render_request_t input;
 digit_render_response_t output;
 unsigned before;
 size_t used;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 stnlabz_module_registry_init(&reg);
 host.register_service=registration;host.unregister_service=unregistration;host.invoke_service=invoke;
 model=digit_expression_descriptor();view=digit_rendering_descriptor();
 check(!strcmp(view.id,"rendering")&&view.required_core_api_minor==STNLABZ_MODULE_API_MINOR,
  "Rendering independent ABI 1.5 descriptor");
 check(stnlabz_module_registry_discover(&reg,&model)==STNLABZ_MODULE_OK,"Model discovered");
 check(stnlabz_module_registry_verify(&reg,"expression")==STNLABZ_MODULE_OK,"Model verified");
 check(stnlabz_module_registry_qualify(&reg,"expression")==STNLABZ_MODULE_OK,"Model qualified");
 check(stnlabz_module_registry_discover(&reg,&view)==STNLABZ_MODULE_OK,"View discovered");
 check(stnlabz_module_registry_verify(&reg,"rendering")==STNLABZ_MODULE_OK,"View verified");
 check(stnlabz_module_registry_qualify(&reg,"rendering")==STNLABZ_MODULE_OK,"View qualified");
 check(stnlabz_module_registry_activate(&reg,"rendering")==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
  "View not automatically activated");
 check(digit_core_authorize(&reg,"expression",&p)==STNLABZ_MODULE_OK,"Core permits Model");
 check(digit_core_activate(&reg,"expression",&host,&p)==STNLABZ_MODULE_OK,"Core activates Model");
 p.module_id="rendering";
 check(digit_core_authorize(&reg,"rendering",&p)==STNLABZ_MODULE_OK,"Core permits View");
 check(digit_core_activate(&reg,"rendering",&host,&p)==STNLABZ_MODULE_OK,"Core activates View");
 check(view_fn!=NULL,"View service registered");
 p.module_id="router";
 check(digit_bootstrap_start(&boot,&reg,&host,&p,"/lookup","expression.lookup"),
  "Bootstrap starts sole Router");
 strcpy(expression.expression,"purrs");
 memset(&input,0,sizeof input);
 input.route.status=digit_bootstrap_handoff(&boot,&request,&model_result,sizeof model_result,&input.route);
 input.model_present=1;input.model=model_result;
 check(input.route.status==DIGIT_ROUTE_OK&&lookups==1,"one Controller to one Model");
 used=0;
 check(view_fn(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_OK &&
 used==sizeof output&&!strcmp(output.text,"purrs: cat vocalization (evidence: fixture:expression-v1)"),
 "View presents known result with evidence");
 check(output.route_status==DIGIT_ROUTE_OK&&lookups==1,"View does not execute or reroute");
 before=lookups;strcpy(expression.expression,"puurs");
 input.route.status=digit_bootstrap_handoff(&boot,&request,&model_result,sizeof model_result,&input.route);
 input.model=model_result;
 check(view_fn(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_OK &&
 !strcmp(output.text,"Expression unknown.")&&output.model_status==DIGIT_EXPRESSION_UNKNOWN,
 "View preserves uncertainty");
 check(lookups==before+1,"unknown expression only one Model call");
 before=lookups;request.core_authorized=0;
 input.route.status=digit_bootstrap_handoff(&boot,&request,&model_result,sizeof model_result,&input.route);
 input.model_present=0;
 check(view_fn(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_OK &&
 !strcmp(output.text,"Request denied.")&&output.route_status==DIGIT_ROUTE_DENIED,
 "View reports denial without overriding Controller");
 check(lookups==before,"View cannot bypass denial");
 request.core_authorized=1;strcpy(request.path,"/unregistered");
 input.route.status=digit_bootstrap_handoff(&boot,&request,&model_result,sizeof model_result,&input.route);
 check(view_fn(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_OK &&
 !strcmp(output.text,"Unknown route."),"Unknown route presented");
 check(lookups==before,"Unknown route does not run Model");
 input.route.status=DIGIT_ROUTE_OK;input.route.service_result=STNLABZ_MODULE_OK;
 input.route.response_used=sizeof input.model;input.model_present=0;
 check(view_fn(&input,sizeof input,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
 "View refuses to manufacture missing Model evidence");
 check(lookups==before,"Invalid rendering never invokes Model");
 check(digit_bootstrap_stop(&boot),"Bootstrap stops Router alone");
 check(view_fn!=NULL&&model_fn!=NULL,"View and Model remain independent of Router lifetime");
 check(digit_core_stop_all(&reg),"Core shuts down Model and View");
 check(view_fn==NULL&&model_fn==NULL,"View and Model unregister");
 check(digit_rendering_render(&input,sizeof input,&output,sizeof output,&used,NULL)==
 STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"Stopped View refuses execution");
 stnlabz_module_registry_destroy(&reg);digit_log_close();
 printf("Core rendering tests: %u executed, %u failed\n",checks,failures);
 return failures!=0;
}
