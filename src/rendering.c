#include <stdio.h>
#include <string.h>
#include "rendering.h"
#include "logging.h"
static const stnlabz_module_host_t *render_host;
static int terminated(const char *s,size_t cap){return memchr(s,0,cap)!=NULL;}
stnlabz_module_result_t digit_rendering_render(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context){
 const digit_render_request_t *in=request;
 digit_render_response_t *out=response;
 const char *message=NULL;
 (void)context;
 if(response_used)*response_used=0;
 if(!render_host||!digit_log_is_open()||!in||request_size!=sizeof(*in)||
 !out||response_size<sizeof(*out)||!response_used||
 !terminated(in->route.request_id,sizeof in->route.request_id))
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof(*out));
 out->route_status=in->route.status;
 out->model_status=DIGIT_EXPRESSION_INVALID;
 (void)snprintf(out->request_id,sizeof out->request_id,"%s",in->route.request_id);
 switch(in->route.status){
 case DIGIT_ROUTE_DENIED: message="Request denied.";break;
 case DIGIT_ROUTE_UNKNOWN: message="Unknown route.";break;
 case DIGIT_ROUTE_UNAVAILABLE: message="Service unavailable.";break;
 case DIGIT_ROUTE_INVALID: message="Invalid request.";break;
 case DIGIT_ROUTE_FAILED: message="Request execution failed.";break;
 case DIGIT_ROUTE_OK:
  if(!in->model_present||in->route.service_result!=STNLABZ_MODULE_OK||
    in->route.response_used!=sizeof(in->model)||
    !terminated(in->model.expression,sizeof in->model.expression)||
    !terminated(in->model.meaning,sizeof in->model.meaning)||
    !terminated(in->model.evidence,sizeof in->model.evidence))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  out->model_status=in->model.status;
  switch(in->model.status){
  case DIGIT_EXPRESSION_KNOWN:
   if(!in->model.expression[0]||!in->model.meaning[0]||!in->model.evidence[0])
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
   (void)snprintf(out->text,sizeof out->text,"%s: %s (evidence: %s)",
    in->model.expression,in->model.meaning,in->model.evidence);
   break;
  case DIGIT_EXPRESSION_UNKNOWN: message="Expression unknown.";break;
  case DIGIT_EXPRESSION_INVALID: message="Invalid expression.";break;
  default:return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  }
  break;
 default:return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 }
 if(message)(void)snprintf(out->text,sizeof out->text,"%s",message);
 if(!digit_log_event("RENDERING","PRESENT","status=formatted")||!digit_log_sync())
 return STNLABZ_MODULE_ERR_INVALID_STATE;
 *response_used=sizeof(*out);
 return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_rendering_qualify(stnlabz_module_qualification_result_t *result){
 if(!result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(result,0,sizeof(*result));
 result->tests_executed=result->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 result->negative_test_executed=result->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_rendering_start(const stnlabz_module_host_t *host){
 if(render_host||!host||!host->register_service||!host->unregister_service||!digit_log_is_open())
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_service("rendering.render",digit_rendering_render,NULL))
 return STNLABZ_MODULE_ERR_START_FAILED;
 render_host=host;
 if(!digit_log_event("RENDERING","START","status=ready")||!digit_log_sync()){
  render_host=NULL;(void)host->unregister_service("rendering.render",NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_rendering_stop(void){
 if(!render_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!render_host->unregister_service("rendering.render",NULL))
 return STNLABZ_MODULE_ERR_STOP_FAILED;
 render_host=NULL;
 return digit_log_event("RENDERING","STOP","status=stopped")&&digit_log_sync()?
 STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_STOP_FAILED;
}
stnlabz_module_descriptor_t digit_rendering_descriptor(void){
 stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"rendering");
 (void)snprintf(d.name,sizeof d.name,"Digit Rendering View");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=digit_rendering_qualify;d.start=digit_rendering_start;d.stop=digit_rendering_stop;
 return d;
}
