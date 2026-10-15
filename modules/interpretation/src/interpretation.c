#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "interpretation.h"
static int active;
static int tok(const char *s,const char *w){
 size_t n=strlen(w),i;for(i=0;s[i];i++){
  size_t j;if(i&&(isalnum((unsigned char)s[i-1])||s[i-1]=='_'))continue;
  for(j=0;j<n&&s[i+j];j++)if(tolower((unsigned char)s[i+j])!=tolower((unsigned char)w[j]))break;
  if(j==n&&!(isalnum((unsigned char)s[i+j])||s[i+j]=='_'))return 1;
 }return 0;
}
static int prefix(const char *s,const char *p){
 size_t i;for(i=0;p[i];i++)if(!s[i]||tolower((unsigned char)s[i])!=tolower((unsigned char)p[i]))return 0;
 return 1;
}
static void trim_subject(const char *s,char *dst,size_t cap){
 size_t n;while(*s==' ')s++;n=strlen(s);
 while(n&&(s[n-1]=='?'||s[n-1]=='!'||s[n-1]=='.'||s[n-1]==' '))n--;
 if(n>=cap)n=cap-1;
 memcpy(dst,s,n);dst[n]=0;
}
static int classify(const char *s,digit_interpret_response_t *out){
 static const char *knowledge[]={"what is ","what are ","who is ","who are ","tell me about ","describe ","explain ","information about ","do you know about ",NULL};
 unsigned i;
 memset(out,0,sizeof *out);out->version=DIGIT_INTERPRETATION_VERSION;
 if((tok(s,"can")||tok(s,"could")||tok(s,"able")||tok(s,"capabilities")||
     tok(s,"functions")||tok(s,"skills")||tok(s,"help"))&&
    (tok(s,"you")||tok(s,"your")||tok(s,"digit")||tok(s,"help"))&&
    (tok(s,"do")||tok(s,"tell")||tok(s,"know")||tok(s,"help")||
     tok(s,"capabilities")||tok(s,"functions")||tok(s,"skills"))){
  out->intent=DIGIT_INTENT_CAPABILITIES;out->confidence=90;return 1;
 }
 /* Explicit conversational intents do not imply knowledge evidence or authority. */
 if(tok(s,"digit")&&(tok(s,"here")||tok(s,"there")||tok(s,"online"))&&
    (strchr(s,'?')||tok(s,"are")||tok(s,"you"))){
  out->intent=DIGIT_INTENT_CHECKIN;out->confidence=85;return 1;
 }
 if(tok(s,"corpus")&&(tok(s,"access")||tok(s,"use")||tok(s,"check")||
                        tok(s,"consult")||tok(s,"look"))){
  out->intent=DIGIT_INTENT_CORPUS_GUIDANCE;out->confidence=85;return 1;
 }
 if((tok(s,"digit")||tok(s,"digit-bot"))&&
    (tok(s,"report")||tok(s,"reports"))&&
    (tok(s,"apache")||tok(s,"picked")||tok(s,"found"))){
  out->intent=DIGIT_INTENT_OBSERVATION;out->confidence=70;return 1;
 }
 for(i=0;knowledge[i];i++)if(prefix(s,knowledge[i])){
  trim_subject(s+strlen(knowledge[i]),out->subject,sizeof out->subject);
  if(!out->subject[0])return 0;
  /* A leading pronoun alone cannot be grounded to a Corpus record. */
  if(!strcmp(out->subject,"you")||!strcmp(out->subject,"me"))return 0;
  out->intent=DIGIT_INTENT_KNOWLEDGE;out->confidence=85;return 1;
 }
 return 0;
}
static stnlabz_module_result_t interpret(const void *request,size_t size,void *response,
 size_t cap,size_t *used,void *ctx){
 const digit_interpret_request_t *q=request;digit_interpret_response_t *out=response;
 (void)ctx;
 if(!active||!q||size!=sizeof *q||!out||cap<sizeof *out||!used||
    q->version!=DIGIT_INTERPRETATION_VERSION||
    !memchr(q->text,0,sizeof q->text)||!q->text[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 (void)classify(q->text,out);*used=sizeof *out;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 const struct {const char *s;digit_intent_t intent;const char *subject;} cases[]={
 {"what can you tell me?",DIGIT_INTENT_CAPABILITIES,""},
 {"What are your capabilities?",DIGIT_INTENT_CAPABILITIES,""},
 {"Could you tell me what you can do?",DIGIT_INTENT_CAPABILITIES,""},
 {"how can you help?",DIGIT_INTENT_CAPABILITIES,""},
 {"tell me about the Chaos Foundation",DIGIT_INTENT_KNOWLEDGE,"the Chaos Foundation"},
 {"what is STN-Labz?",DIGIT_INTENT_KNOWLEDGE,"STN-Labz"},
 {"describe Sentinel",DIGIT_INTENT_KNOWLEDGE,"Sentinel"},
 {"explain Corpus",DIGIT_INTENT_KNOWLEDGE,"Corpus"},
 {"describe",DIGIT_INTENT_UNKNOWN,""},
 {"erase the logs",DIGIT_INTENT_UNKNOWN,""},
 {"learn everything",DIGIT_INTENT_UNKNOWN,""},
 {"deploy an update",DIGIT_INTENT_UNKNOWN,""},
 {"tell me about",DIGIT_INTENT_UNKNOWN,""},
 {"who is?",DIGIT_INTENT_UNKNOWN,""},
 {"mmhmmm trying to fix this, digit you here?",DIGIT_INTENT_CHECKIN,""},
 {"Stupid digit, you need to access your corpus",DIGIT_INTENT_CORPUS_GUIDANCE,""},
 {"Interesting, same report on Apache from days ago, Digit picked them up too",DIGIT_INTENT_OBSERVATION,""}
 };
 unsigned i;digit_interpret_response_t out;memset(q,0,sizeof *q);
 for(i=0;i<sizeof cases/sizeof cases[0];i++){
  (void)classify(cases[i].s,&out);q->tests_executed++;
  if(out.intent==cases[i].intent&&!strcmp(out.subject,cases[i].subject))q->tests_passed++;
  else q->tests_failed++;
 }
 q->negative_test_executed=1;q->negative_test_passed=(q->tests_failed==0);
 return q->tests_failed?STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(active||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("interpretation.classify",interpret,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 active=1;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_host_t *host;
static stnlabz_module_result_t activate(const stnlabz_module_host_t *h){
 stnlabz_module_result_t r=start(h);if(r==STNLABZ_MODULE_OK)host=h;return r;
}
static stnlabz_module_result_t stop(void){
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("interpretation.classify",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 active=0;host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"interpretation");snprintf(d.name,sizeof d.name,"Digit Interpretation");
 d.version_major=0;d.version_minor=1;d.version_patch=1;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=activate;d.stop=stop;return &d;
}
