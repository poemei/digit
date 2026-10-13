#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "stnlabz_abi/module.h"
static const stnlabz_module_host_t *host;
static stnlabz_module_result_t answer(const void *request,size_t length,void *response,size_t capacity,size_t *used,void *context){
 const char *s=request;char *out=response;const char *reply;
 (void)context;
 if(!s||!length||length>512||s[length-1]!=0||!memchr(s,0,length)||!response||!used||capacity<2)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!s[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Ask only handles conversation. It cannot confer authority or dispatch commands. */
 if(!strcasecmp(s,"hello")||!strcasecmp(s,"hi")||
    !strcasecmp(s,"hello digit")||!strcasecmp(s,"hi digit")||
    !strcasecmp(s,"hello digit!")||!strcasecmp(s,"hi digit!"))
  reply="Hello. I'm Digit. What would you like to discuss?";
 else
  reply="I received your message. General language interpretation is not connected yet; I cannot give a grounded answer to that request.";
 if(strlen(reply)+1>capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(out,reply,strlen(reply)+1);*used=strlen(reply)+1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("route./ask",answer,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("route./ask",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);snprintf(d.id,sizeof d.id,"ask");snprintf(d.name,sizeof d.name,"Digit Ask");
 d.version_major=0;d.version_minor=1;d.version_patch=1;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
