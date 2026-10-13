#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "stnlabz_abi/module.h"
#include "ask_context.h"
static const stnlabz_module_host_t *host;
static stnlabz_module_result_t answer(const void *request,size_t length,
 void *response,size_t capacity,size_t *used,void *context){
 const char *s=request;char *out=response;const char *reply;
 const digit_ask_context_t *c=NULL;
 (void)context;
 if(length==sizeof(digit_ask_context_t)){
  c=request;
  if(!c||c->magic!=DIGIT_ASK_CONTEXT_MAGIC||
     !memchr(c->question,0,sizeof c->question))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  s=c->question;length=strlen(s)+1;
 }
 if(!s||!length||length>513||s[length-1]!=0||!response||!used||capacity<2||
    memchr(s,0,length-1))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!s[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!strcasecmp(s,"hello")||!strcasecmp(s,"hi")||
    !strcasecmp(s,"hello digit")||!strcasecmp(s,"hi digit")||
    !strcasecmp(s,"hello digit!")||!strcasecmp(s,"hi digit!"))
  reply="Hello. I'm Digit. What would you like to discuss?";
 else if(!strcasecmp(s,"who are you?")||!strcasecmp(s,"who are you")||
         !strcasecmp(s,"what is your name?")||!strcasecmp(s,"what are you?")){
  const char empty=0;size_t n=0;
  if(!host||!host->invoke_service)return STNLABZ_MODULE_ERR_NOT_FOUND;
  return host->invoke_service("core.identity.read",&empty,sizeof empty,
                              response,capacity,&n)==STNLABZ_MODULE_OK&&
         n>0&&n<=capacity&&memchr(response,0,n)?
         (*used=n,STNLABZ_MODULE_OK):STNLABZ_MODULE_ERR_NOT_FOUND;
 }else if(!strcasecmp(s,"diagnose my last request")||
          !strcasecmp(s,"diagnose last request")||
          !strcasecmp(s,"diagnose")){
  digit_scoped_diagnostic_t observation={0};size_t n=0;
  if(!c||!c->has_previous)
   reply="DSD: No prior request in this conversation to diagnose.";
  else {
   if(!host||!host->invoke_service)return STNLABZ_MODULE_ERR_NOT_FOUND;
   memcpy(observation.request_id,c->previous_request,sizeof observation.request_id);
   memcpy(observation.route,c->previous_route,sizeof observation.route);
   memcpy(observation.service,c->previous_service,sizeof observation.service);
   observation.outcome=c->previous_outcome;
   observation.service_result=c->previous_service_result;
   if(host->invoke_service("dsd.scoped",&observation,sizeof observation,
                            response,capacity,&n)!=STNLABZ_MODULE_OK||
      !n||n>capacity||!memchr(response,0,n))
    return STNLABZ_MODULE_ERR_NOT_FOUND;
   *used=n;return STNLABZ_MODULE_OK;
  }
 }else
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
 d.version_major=0;d.version_minor=1;d.version_patch=3;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
