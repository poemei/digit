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
/* Deterministic speech-act fallback: classification is not authorization.
 * No factual claim is derived here; Corpus remains the sole knowledge source. */
static digit_intent_t speech_act(const char *s){
 size_t words=0;int in_word=0;const unsigned char *p;
 if(!s||!*s)return DIGIT_INTENT_AMBIGUOUS;
 for(p=(const unsigned char *)s;*p;p++){
  if(isalnum(*p)||*p=='_'){if(!in_word)words++;in_word=1;}
  else in_word=0;
 }
 if(prefix(s,"please ")||prefix(s,"could you ")||prefix(s,"would you ")||
    prefix(s,"can you ")||prefix(s,"i need you to "))
  return DIGIT_INTENT_REQUEST;
 if(prefix(s,"delete ")||prefix(s,"erase ")||prefix(s,"deploy ")||
    prefix(s,"update ")||prefix(s,"open ")||prefix(s,"check ")||
    prefix(s,"access ")||prefix(s,"read ")||prefix(s,"learn "))
  return DIGIT_INTENT_INSTRUCTION;
 if(strchr(s,'?')||prefix(s,"why ")||prefix(s,"how ")||
    prefix(s,"when ")||prefix(s,"where ")||prefix(s,"what ")||
    prefix(s,"who ")||prefix(s,"is ")||prefix(s,"are "))
  return DIGIT_INTENT_QUESTION;
 if(words>=3)return DIGIT_INTENT_STATEMENT;
 return DIGIT_INTENT_AMBIGUOUS;
}
static int classify(const char *s,const char *evidence,digit_interpret_response_t *out){
 static const char *knowledge[]={"what is ","what are ","who is ","who are ","tell me about ","describe ","explain ","information about ","do you know about ",NULL};
 unsigned i;
 /* Preserve address semantics while classifying the actual utterance. */
 const char *utterance=s;
 memset(out,0,sizeof *out);out->version=DIGIT_INTERPRETATION_VERSION;
 if(prefix(utterance,"digit-bot") &&
    (utterance[9]==':'||utterance[9]==','||utterance[9]==' ')){
   utterance+=9;
 }else if(prefix(utterance,"digit") &&
    (utterance[5]==':'||utterance[5]==','||utterance[5]==' ')){
   utterance+=5;
 }
 while(*utterance==' '||*utterance==':'||*utterance==',')utterance++;
 if(!*utterance)utterance=s;
 /* Direct address may occur after ordinary conversational text. */
 out->addressed=(unsigned)(utterance!=s||tok(s,"digit")||tok(s,"digit-bot"));
 /* Interpretation classifies a greeting using learned Corpus evidence.
  * Corpus retrieval remains owned by the caller's model service. */
 if(evidence&&*evidence&&
    tok(evidence,"greeting")){
  size_t n=strlen(utterance);
  while(n&&(utterance[n-1]=='!'||utterance[n-1]=='.'||utterance[n-1]==' '))n--;
  if(n&&n<sizeof out->subject){
   memcpy(out->subject,utterance,n);out->subject[n]=0;
   out->intent=DIGIT_INTENT_GREETING;out->confidence=80;return 1;
  }
 }
 if((tok(s,"can")||tok(s,"could")||tok(s,"able")||tok(s,"capabilities")||
     tok(s,"functions")||tok(s,"skills")||tok(s,"help"))&&
    (tok(s,"you")||tok(s,"your")||tok(s,"digit")||tok(s,"help"))&&
    (tok(s,"do")||tok(s,"tell")||tok(s,"know")||tok(s,"help")||
     tok(s,"capabilities")||tok(s,"functions")||tok(s,"skills"))){
  out->intent=DIGIT_INTENT_CAPABILITIES;out->confidence=90;return 1;
 }
 /* Explicit conversational intents do not imply knowledge evidence or authority. */
 if((tok(s,"digit")||utterance!=s)&&(tok(utterance,"here")||tok(utterance,"there")||tok(utterance,"online"))&&
    (strchr(utterance,'?')||tok(utterance,"are")||tok(utterance,"you"))){
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
 for(i=0;knowledge[i];i++)if(prefix(utterance,knowledge[i])){
  trim_subject(utterance+strlen(knowledge[i]),out->subject,sizeof out->subject);
  if(!out->subject[0])return 0;
  /* A leading pronoun alone cannot be grounded to a Corpus record. */
  if(!strcmp(out->subject,"you")||!strcmp(out->subject,"me"))return 0;
  out->intent=DIGIT_INTENT_KNOWLEDGE;out->confidence=85;return 1;
 }
 out->intent=speech_act(utterance);
 out->confidence=0; /* Speech-act shape is not evidence of actionable intent. */
 return out->intent!=DIGIT_INTENT_AMBIGUOUS;
}
static stnlabz_module_result_t interpret(const void *request,size_t size,void *response,
 size_t cap,size_t *used,void *ctx){
 const digit_interpret_request_t *q=request;digit_interpret_response_t *out=response;
 (void)ctx;
 if(!active||!q||size!=sizeof *q||!out||cap<sizeof *out||!used||
    q->version!=DIGIT_INTERPRETATION_VERSION||
    !memchr(q->text,0,sizeof q->text)||!q->text[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!memchr(q->corpus_text,0,sizeof q->corpus_text))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 (void)classify(q->text,q->corpus_text,out);*used=sizeof *out;return STNLABZ_MODULE_OK;
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
 {"describe",DIGIT_INTENT_AMBIGUOUS,""},
 {"erase the logs",DIGIT_INTENT_INSTRUCTION,""},
 {"learn everything",DIGIT_INTENT_INSTRUCTION,""},
 {"deploy an update",DIGIT_INTENT_INSTRUCTION,""},
 {"tell me about",DIGIT_INTENT_STATEMENT,""},
 {"who is?",DIGIT_INTENT_QUESTION,""},
 {"mmhmmm trying to fix this, digit you here?",DIGIT_INTENT_CHECKIN,""},
 {"Stupid digit, you need to access your corpus",DIGIT_INTENT_CORPUS_GUIDANCE,""},
 {"Interesting, same report on Apache from days ago, Digit picked them up too",DIGIT_INTENT_OBSERVATION,""},
 {"mmhmm and hopefully digit doesnt give me her canned bullshit",DIGIT_INTENT_STATEMENT,""},
 {"Could you check Apache?",DIGIT_INTENT_REQUEST,""},
 {"Why is Apache mentioned?",DIGIT_INTENT_QUESTION,""},
 {"delete all the records",DIGIT_INTENT_INSTRUCTION,""},
 {"hmm",DIGIT_INTENT_AMBIGUOUS,""},
 {"Digit you here?",DIGIT_INTENT_CHECKIN,""},
 {"digit, you here?",DIGIT_INTENT_CHECKIN,""},
 {"Digit: explain PHP",DIGIT_INTENT_KNOWLEDGE,"PHP"},
 {"digit explain PHP",DIGIT_INTENT_KNOWLEDGE,"PHP"}
 };
 unsigned i;digit_interpret_response_t out;memset(q,0,sizeof *q);
 for(i=0;i<sizeof cases/sizeof cases[0];i++){
  (void)classify(cases[i].s,"",&out);q->tests_executed++;
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
 d.version_major=0;d.version_minor=1;d.version_patch=4;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=activate;d.stop=stop;return &d;
}
