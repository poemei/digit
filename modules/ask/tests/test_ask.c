#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "stnlabz_abi/module.h"
#include "ask_context.h"
#include "corpus_lookup.h"
#include "interpretation.h"
#include "research_contract.h"
#include "observation.h"
#include "reasoning.h"
#include "socials.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int identity_calls;
static int socials_available=1;
static int socials_calls;
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t size,void *reply,size_t cap,size_t *used){
 const char *msg="I am Digit, the STN-LABZ foundational autonomous agent.";
 if(!strcmp(name,DIGIT_SOCIALS_SERVICE)){
  const digit_socials_request_t *q=req;
  digit_socials_response_t *r=reply;
  socials_calls++;
  if(!socials_available)return STNLABZ_MODULE_ERR_NOT_FOUND;
  if(size!=sizeof *q||cap<sizeof *r||q->version!=DIGIT_SOCIALS_VERSION||
     q->category!=DIGIT_SOCIALS_ANY||!q->context[0])
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(r,0,sizeof *r);
  r->version=DIGIT_SOCIALS_VERSION;r->category=DIGIT_SOCIALS_HEART;
  strcpy(r->emoji,"💚");*used=sizeof *r;
  return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,"interpretation.classify")){
  const digit_interpret_request_t *q=req;
  digit_interpret_response_t *v=reply;
  if(size!=sizeof *q||cap<sizeof *v)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(v,0,sizeof *v);v->version=DIGIT_INTERPRETATION_VERSION;
  if(strstr(q->corpus_text,"greeting")){
   v->intent=DIGIT_INTENT_GREETING;
   if(!strncasecmp(q->text,"Digit ",6))snprintf(v->subject,sizeof v->subject,"%s",q->text+6);
   else snprintf(v->subject,sizeof v->subject,"%s",q->text);
  }
  else if(!strcmp(q->text,"what can you tell me?"))v->intent=DIGIT_INTENT_CAPABILITIES;
  else if(!strcmp(q->text,"digit you here?"))v->intent=DIGIT_INTENT_CHECKIN;
  else if(!strcmp(q->text,"digit, access your corpus"))v->intent=DIGIT_INTENT_CORPUS_GUIDANCE;
  else if(!strcmp(q->text,"digit picked up that report"))v->intent=DIGIT_INTENT_OBSERVATION;
  else if(strstr(q->text,"Digit")||strstr(q->text,"digit"))v->addressed=1;
  else if(!strcmp(q->text,"tell me about STN-Labz")){
   v->intent=DIGIT_INTENT_KNOWLEDGE;strcpy(v->subject,"STN-Labz");
  }
  *used=sizeof *v;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_OBSERVATION_READ_SERVICE)){
  const digit_observation_recent_request_t *q=req;
  digit_observation_recent_response_t *r=reply;
  if(size!=sizeof *q||cap<sizeof *r||strcmp(q->transport,"irc")||
     strcmp(q->channel,"#stn-labz"))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(r,0,sizeof *r);r->version=DIGIT_OBSERVATION_READ_VERSION;r->count=1;
  strcpy(r->items[0].event_id,"event-1");
  strcpy(r->items[0].sender,"STN_Boss");
  strcpy(r->items[0].text,"so I added a reasoning module for Digit");
  *used=sizeof *r;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_REASONING_SERVICE)){
  const digit_reasoning_request_t *q=req;
  digit_reasoning_response_t *r=reply;
  if(size!=sizeof *q||cap<sizeof *r||strcmp(q->channel,"#stn-labz")||
     strcmp(q->sender,"PoeMei")||q->observation_count!=1||
     strcmp(q->observations[0].event_id,"event-1"))
     return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(r,0,sizeof *r);r->version=DIGIT_REASONING_VERSION;
  r->response_eligible=q->addressed;
  r->knowledge=q->corpus_state==DIGIT_REASONING_PRESENT?
    DIGIT_REASONING_CORPUS_SUPPORTED:DIGIT_REASONING_UNKNOWN;
  r->relevance=DIGIT_REASONING_CONTEXTUAL;r->matched_observations=1;
  strcpy(r->observation_ids[0],"event-1");
  *used=sizeof *r;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,"core.version.read")){
  const char *version="1.0.0";
  if(size!=1||*(const char *)req!=0||cap<strlen(version)+1)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(reply,version,strlen(version)+1);*used=strlen(version)+1;
  return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_RESEARCH_SERVICE)){
  const digit_research_request_t *q=req;
  digit_research_result_t *r=reply;
  if(size!=sizeof *q||cap<sizeof *r||q->version!=DIGIT_RESEARCH_CONTRACT_VERSION)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(r,0,sizeof *r);
  r->version=DIGIT_RESEARCH_CONTRACT_VERSION;
  r->http_status=200;
  strcpy(r->retrieved_at,"2026-10-14T00:00:00Z");
  snprintf(r->url,sizeof r->url,"%s",q->url);
  strcpy(r->excerpt,"TEST SOURCE ONLY: release notes");
  *used=sizeof *r;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,"corpus.lookup")){
  const digit_corpus_lookup_request_t *q=req;
  digit_corpus_lookup_response_t *result=reply;
  if(size!=sizeof *q||cap<sizeof *result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(result,0,sizeof *result);
  if(!strcasecmp(q->term,"hello")||!strcasecmp(q->term,"hi")||
     !strcasecmp(q->term,"good morning")){
   result->found=1;
   strcpy(result->id,"TEST-GREETING");
   strcpy(result->source,"operator-learned");
   snprintf(result->text,sizeof result->text,"%s is a greeting commonly used to open a conversation.",q->term);
  }
  if(!strcmp(q->term,"STN-Labz")){
   result->found=1;
   strcpy(result->id,"TEST-ORG");
   strcpy(result->source,"fixture-corpus");
   strcpy(result->text,"STN-LABZ engineering research organization.");
  }
  *used=sizeof *result;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,"dsd.scoped")){
  const digit_scoped_diagnostic_t *d=req;
  const char *m="DSD SCOPED REPORT";
  if(size!=sizeof *d||!d->request_id[0]||strlen(m)+1>cap)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(reply,m,strlen(m)+1);*used=strlen(m)+1;return STNLABZ_MODULE_OK;
 }
 if(strcmp(name,"core.identity.read")||size!=1||*(const char *)req!=0||strlen(msg)+1>cap)return STNLABZ_MODULE_ERR_NOT_FOUND;
 identity_calls++;memcpy(reply,msg,strlen(msg)+1);*used=strlen(msg)+1;return STNLABZ_MODULE_OK;
}
static int reg(const char *s,stnlabz_module_service_handler_fn fn,void *ctx){(void)ctx;if(strcmp(s,"route./ask"))return 0;handler=fn;return 1;}
static int unreg(const char *s,void *ctx){(void)ctx;return strcmp(s,"route./ask")==0;}
int main(void){
 stnlabz_module_qualification_result_t q;char out[512];size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t h={0};h.register_service=reg;h.unregister_service=unreg;h.invoke_service=invoke;
 if(strcmp(d->id,"ask")||d->qualify(&q)!=STNLABZ_MODULE_OK||d->start(&h)!=STNLABZ_MODULE_OK||!handler)return 1;
 if(handler("hello",6,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"hello"))return 1;
 if(handler("hi",3,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"hi"))return 1;
 if(d->version_minor!=2||d->version_patch!=5)return 1;
 if(socials_calls!=2||!strstr(out,"💚"))return 1;
 socials_available=0;
 if(handler("hello",6,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    strcmp(out,"hello"))return 1;
 socials_available=1;
 if(handler("Digit good morning",sizeof("Digit good morning"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"good morning")||!strstr(out,"💚")||strstr(out,"Corpus:"))return 1;
 if(handler("Digit good unknown",sizeof("Digit good unknown"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"clarify"))return 1;
 if(handler("what version are you?",22,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Digit 1.0.0"))return 1;
 if(handler("version",8,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Digit 1.0.0"))return 1;
 if(handler("who are you?",13,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"foundational autonomous agent")||identity_calls!=1)return 1;
 {digit_ask_context_t context={0};context.magic=DIGIT_ASK_CONTEXT_MAGIC;
  strcpy(context.question,"diagnose my last request");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"No prior request"))return 1;
  context.has_previous=1;strcpy(context.previous_request,"http-1");strcpy(context.previous_route,"/ask");strcpy(context.previous_service,"route./ask");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"DSD SCOPED REPORT"))return 1;
 }
 if(handler("what is STN-Labz?",18,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"engineering research organization")||
     !strstr(out,"fixture-corpus"))return 1;
 if(handler("what is unlisted?",18,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"no matching knowledge record"))return 1;
 if(handler("what can you tell me?",22,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"Corpus"))return 1;
 if(handler("tell me about STN-Labz",23,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"engineering research organization"))return 1;
 if(handler("research PHP",sizeof("research PHP"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"https://www.php.net/releases/")||
     !strstr(out,"Unverified source excerpt"))return 1;
 if(handler("research https://www.php.net/releases/",sizeof("research https://www.php.net/releases/"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"Retrieved source:"))return 1;
 if(handler("research unknown subject",sizeof("research unknown subject"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"not available"))return 1;
 if(handler("digit hello",sizeof("digit hello"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"hello"))return 1;
 if(handler("digit you here?",sizeof("digit you here?"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"I'm here"))return 1;
 if(handler("digit, access your corpus",sizeof("digit, access your corpus"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"specific subject"))return 1;
 if(handler("digit picked up that report",sizeof("digit picked up that report"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"have not verified"))return 1;
 if(handler("delete all the records",23,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0)return 1;
 {digit_ask_context_t context={0};context.magic=DIGIT_ASK_CONTEXT_MAGIC;
  strcpy(context.transport,"irc");strcpy(context.channel,"#stn-labz");
  strcpy(context.sender,"PoeMei");
  strcpy(context.question,"I added a reasoning module for Digit, and expect a canned reply");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     used!=1||out[0]!=0)return 1;
  strcpy(context.question,"Digit I fixed the problem");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"clarify"))return 1;
  strcpy(context.question,"Can Digit explain a technology that Corpus doesn't contain?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"I do not know"))return 1;
  strcpy(context.channel,"#other-channel");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK)
   return 1;
 }
 if(handler("",1,out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK)return 1;
 if(handler("hello",6,out,2,&used,NULL)==STNLABZ_MODULE_OK)return 1;
 if(d->stop()!=STNLABZ_MODULE_OK)return 1;
 puts("Ask tests: PASS");return 0;
}
