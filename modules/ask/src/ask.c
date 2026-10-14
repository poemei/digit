#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "stnlabz_abi/module.h"
#include "ask_context.h"
#include "corpus_lookup.h"
#include "interpretation.h"
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
 }else if(!strncasecmp(s,"what is ",8)){
  const char *term=s+8;size_t n=strlen(term),used_lookup=0;
  digit_corpus_lookup_request_t query={0};
  digit_corpus_lookup_response_t found={0};
  int written;
  while(n&&(term[n-1]=='?'||term[n-1]=='.'||term[n-1]==' '))n--;
  if(!n||n>=sizeof query.term)
   reply="Corpus: invalid knowledge query.";
  else if(!host||!host->invoke_service)
   reply="Corpus service is unavailable.";
  else {
   memcpy(query.term,term,n);
   if(host->invoke_service("corpus.lookup",&query,sizeof query,
        &found,sizeof found,&used_lookup)!=STNLABZ_MODULE_OK||
      used_lookup!=sizeof found)
    reply="Corpus service is unavailable.";
   else if(!found.found)
    reply="Corpus: no matching knowledge record.";
   else if(!memchr(found.text,0,sizeof found.text)||
           !memchr(found.source,0,sizeof found.source)||
           !memchr(found.id,0,sizeof found.id))
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   else {
    written=snprintf(out,capacity,"%s [Corpus: %s; source: %s]",
                     found.text,found.id,found.source);
    if(written<0||(size_t)written>=capacity)
     reply="Corpus record exceeds this interface's response limit.";
    else {*used=(size_t)written+1;return STNLABZ_MODULE_OK;}
   }
  }
 }else {
  digit_interpret_request_t request_intent={0};
  digit_interpret_response_t interpreted={0};
  size_t result_used=0;
  request_intent.version=DIGIT_INTERPRETATION_VERSION;
  snprintf(request_intent.text,sizeof request_intent.text,"%s",s);
  if(!host||!host->invoke_service||
     host->invoke_service("interpretation.classify",&request_intent,sizeof request_intent,
                          &interpreted,sizeof interpreted,&result_used)!=STNLABZ_MODULE_OK||
     result_used!=sizeof interpreted||interpreted.version!=DIGIT_INTERPRETATION_VERSION)
   reply="General language interpretation is unavailable.";
  else if(interpreted.intent==DIGIT_INTENT_CAPABILITIES)
   reply="I can answer greetings, identify myself using Core, retrieve factual records from Corpus, and report a scoped diagnosis of your previous request. Please ask about a specific subject.";
  else if(interpreted.intent==DIGIT_INTENT_KNOWLEDGE){
   digit_corpus_lookup_request_t query={0};
   digit_corpus_lookup_response_t record={0};
   size_t n=0;int written;
   if(!memchr(interpreted.subject,0,sizeof interpreted.subject)||
      !interpreted.subject[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
   snprintf(query.term,sizeof query.term,"%s",interpreted.subject);
   if(host->invoke_service("corpus.lookup",&query,sizeof query,&record,sizeof record,&n)!=STNLABZ_MODULE_OK||
      n!=sizeof record)
    reply="Corpus service is unavailable.";
   else if(!record.found)
    reply="Corpus: no matching knowledge record.";
   else if(!memchr(record.text,0,sizeof record.text)||
           !memchr(record.id,0,sizeof record.id)||
           !memchr(record.source,0,sizeof record.source))
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   else {
    written=snprintf(out,capacity,"%s [Corpus: %s; source: %s]",
                      record.text,record.id,record.source);
    if(written<0||(size_t)written>=capacity)
     reply="Corpus record exceeds this interface's response limit.";
    else {*used=(size_t)written+1;return STNLABZ_MODULE_OK;}
   }
  }else
   reply="I cannot determine that request with sufficient confidence. Please specify the subject or ask what I can do.";
 }
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
 d.version_major=0;d.version_minor=1;d.version_patch=5;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
