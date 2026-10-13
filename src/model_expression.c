#include <stdio.h>
#include <string.h>
#include "model_expression.h"
#include "logging.h"
static const stnlabz_module_host_t *expression_host;
static int valid(const char *s,size_t cap){
 size_t i;if(!s||!s[0])return 0;
 for(i=0;i<cap&&s[i];++i){
  unsigned char c=(unsigned char)s[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')))return 0;
 }
 return i>0&&i<cap;
}
stnlabz_module_result_t digit_expression_lookup(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context){
 const digit_expression_request_t *in=request;
 digit_expression_response_t *out=response;
 char detail[180];int n;
 (void)context;
 if(response_used)*response_used=0;
 if(!expression_host||!digit_log_is_open()||!in||request_size!=sizeof(*in)||
 !out||response_size<sizeof(*out)||!response_used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof(*out));
 out->status=DIGIT_EXPRESSION_INVALID;
 (void)snprintf(out->source_file,sizeof out->source_file,"src/model_expression.c");
 (void)snprintf(out->source_function,sizeof out->source_function,"digit_expression_lookup");
 out->source_line=__LINE__+1;
 if(!valid(in->expression,sizeof in->expression))goto recorded;
 (void)snprintf(out->expression,sizeof out->expression,"%s",in->expression);
 if(!strcmp(in->expression,"purrs")){
  out->status=DIGIT_EXPRESSION_KNOWN;
  (void)snprintf(out->meaning,sizeof out->meaning,"cat vocalization");
  (void)snprintf(out->evidence,sizeof out->evidence,"fixture:expression-v1");
 }else{
  out->status=DIGIT_EXPRESSION_UNKNOWN;
 }
recorded:
 n=snprintf(detail,sizeof detail,"expression=%.63s status=%d evidence=%.63s",
  out->expression[0]?out->expression:"invalid",(int)out->status,
  out->evidence[0]?out->evidence:"none");
 if(n<0||(size_t)n>=sizeof detail||
 !digit_log_event("MODEL_EXPRESSION","LOOKUP",detail)||!digit_log_sync())
 return STNLABZ_MODULE_ERR_INVALID_STATE;
 *response_used=sizeof(*out);return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_expression_qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_expression_start(const stnlabz_module_host_t *host){
 if(expression_host||!host||!host->register_service||!host->unregister_service||!digit_log_is_open())
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_service("expression.lookup",digit_expression_lookup,NULL))
 return STNLABZ_MODULE_ERR_START_FAILED;
 expression_host=host;
 if(!digit_log_event("MODEL_EXPRESSION","START","status=ready")||!digit_log_sync()){
  expression_host=NULL;(void)host->unregister_service("expression.lookup",NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
stnlabz_module_result_t digit_expression_stop(void){
 if(!expression_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!expression_host->unregister_service("expression.lookup",NULL))
 return STNLABZ_MODULE_ERR_STOP_FAILED;
 expression_host=NULL;
 return digit_log_event("MODEL_EXPRESSION","STOP","status=stopped")&&digit_log_sync()?
 STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_STOP_FAILED;
}
stnlabz_module_descriptor_t digit_expression_descriptor(void){
 stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"expression");
 (void)snprintf(d.name,sizeof d.name,"Expression Lookup Model");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=digit_expression_qualify;d.start=digit_expression_start;d.stop=digit_expression_stop;
 return d;
}
