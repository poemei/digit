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
#include "recognition.h"
#include "socials.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int identity_calls;
static int retained_full_message;
static int selected_subject;
static int socials_available=1;
static int socials_calls;
static int ambient_fixture;
static int long_corpus_fixture;
static int recognition_calls;
static int recognition_unrelated;
static int social_repeat_fixture;
static int observation_lookup_calls;
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t size,void *reply,size_t cap,size_t *used){
 const char *msg="I am Digit, the STN-LABZ foundational autonomous agent.";
 if(!strcmp(name,DIGIT_CORPUS_MULTI_SERVICE)){
  const digit_corpus_multi_request_t *q=req;
  digit_corpus_multi_response_t *v=reply;
  if(size!=sizeof *q||cap<sizeof *v||q->version!=DIGIT_CORPUS_MULTI_VERSION)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(v,0,sizeof *v);v->version=DIGIT_CORPUS_MULTI_VERSION;
  if(strstr(q->term,"agenda"))observation_lookup_calls++;
  if(strstr(q->term,"ChAoS MVC Net")){
   v->count=2;
   v->items[0].found=v->items[1].found=1;
   strcpy(v->items[0].id,"T-1");strcpy(v->items[1].id,"T-2");
   strcpy(v->items[0].text,"Context about the ChAoS MVC Net.");
   strcpy(v->items[1].text,"Additional discussion about the ChAoS MVC Net.");
  }
  *used=sizeof *v;return STNLABZ_MODULE_OK;
 }
  if(!strcmp(name,DIGIT_RECOGNITION_SERVICE)){
  const digit_recognition_request_t *q=req;
  digit_recognition_response_t *v=reply;
  if(size!=sizeof *q||cap<sizeof *v||
     q->version!=DIGIT_RECOGNITION_VERSION||!q->sender[0]||
     !q->message[0]||q->history_count>DIGIT_RECOGNITION_HISTORY_MAX)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  recognition_calls++;
  memset(v,0,sizeof *v);v->version=DIGIT_RECOGNITION_VERSION;
  v->direct_address=q->addressed;
  v->corpus_relation=recognition_unrelated?
    DIGIT_RECOGNITION_UNCERTAIN:DIGIT_RECOGNITION_RELATED;
  v->conversation_relation=recognition_unrelated?
    DIGIT_RECOGNITION_UNCERTAIN:DIGIT_RECOGNITION_RELATED;
  *used=sizeof *v;return STNLABZ_MODULE_OK;
 }
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
  v->intent=DIGIT_INTENT_AMBIGUOUS;
  if(strstr(q->corpus_text,"greeting")){
   v->intent=DIGIT_INTENT_GREETING;
   if(!strncasecmp(q->text,"Digit ",6))snprintf(v->subject,sizeof v->subject,"%.*s",(int)sizeof v->subject-1,q->text+6);
   else snprintf(v->subject,sizeof v->subject,"%.*s",(int)sizeof v->subject-1,q->text);
  }
  else if(!strcmp(q->text,"I added a reasoning module for Digit, and expect a canned reply")){
   v->addressed=0;v->intent=DIGIT_INTENT_STATEMENT;
  }
  else if(!strcmp(q->text,"I suppose I'll get Codex ready for tomorrow's ChAoS MVC Net")){
   v->addressed=0;v->intent=DIGIT_INTENT_STATEMENT;
   strcpy(v->subject,"tomorrow's ChAoS MVC Net");
  }
  else if(!strcmp(q->text,"is digit even seeing Das is goods!")){
   v->addressed=0;v->intent=DIGIT_INTENT_QUESTION;
  }
  else if(!strcmp(q->text,"What is STN-Labz?")){
   v->addressed=0;v->intent=DIGIT_INTENT_KNOWLEDGE;
   strcpy(v->subject,"STN-Labz");
  }
  else if(!strcasecmp(q->text,"Digit what is your mission?")){
   v->addressed=1;v->intent=DIGIT_INTENT_KNOWLEDGE;
   strcpy(v->subject,"Digit's mission");
  }
  else if(!strcasecmp(q->text,"digit, what is your mission?")){
   v->addressed=1;v->intent=DIGIT_INTENT_KNOWLEDGE;
   strcpy(v->subject,"Digit's mission");
  }
  else if(!strcmp(q->text,"Digit topic discussion")){
   v->addressed=1;v->intent=DIGIT_INTENT_STATEMENT;
   strcpy(v->subject,"STN-Labz");
  }
  else if(!strcmp(q->text,"what can you tell me?"))v->intent=DIGIT_INTENT_CAPABILITIES;
  else if(!strcmp(q->text,"digit you here?"))v->intent=DIGIT_INTENT_CHECKIN;
  else if(!strcmp(q->text,"digit, access your corpus"))v->intent=DIGIT_INTENT_CORPUS_GUIDANCE;
  else if(!strcmp(q->text,"digit picked up that report"))v->intent=DIGIT_INTENT_OBSERVATION;
  else if(!strncasecmp(q->text,"Digit ",6)){
   v->addressed=1;v->intent=DIGIT_INTENT_AMBIGUOUS;
   snprintf(v->subject,sizeof v->subject,"%s",q->text+6);
  }
  else if(!strcasecmp(q->text,"Hi Digit")){
   v->addressed=1;v->intent=DIGIT_INTENT_AMBIGUOUS;
   strcpy(v->subject,"Hi");
  }
  else if(strstr(q->text,"Digit")||strstr(q->text,"digit"))v->addressed=1;
  else if(!strcmp(q->text,"tell me about STN-Labz")){
   v->intent=DIGIT_INTENT_KNOWLEDGE;strcpy(v->subject,"STN-Labz");
  }
  *used=sizeof *v;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_OBSERVATION_READ_SERVICE)){
  const digit_observation_recent_request_t *q=req;
  digit_observation_recent_response_t *r=reply;
  if(size!=sizeof *q||cap<sizeof *r||
     !((!strcmp(q->transport,"irc")&&!strcmp(q->channel,"#stn-labz"))||
       (!strcmp(q->transport,"slack")&&!strcmp(q->channel,"T1:C1"))))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memset(r,0,sizeof *r);r->version=DIGIT_OBSERVATION_READ_VERSION;r->count=1;
  strcpy(r->items[0].event_id,"event-1");
  strcpy(r->items[0].sender,"STN_Boss");
  strcpy(r->items[0].text,"so I added a reasoning module for Digit");
  if(social_repeat_fixture){
   strcpy(r->items[0].sender,"digit-bot");
   strcpy(r->items[0].text,"Mew!");
  }
   if(ambient_fixture==3){
   r->count=1;
   strcpy(r->items[0].text,"The ChAoS MVC Net concerns company coordination.");
  }else if(ambient_fixture){
   r->count=ambient_fixture==2?1:2;
   strcpy(r->items[0].text,"ChAoS MVC Net agenda has been discussed");
   strcpy(r->items[1].event_id,"event-2");
   strcpy(r->items[1].sender,"STN_Boss");
   strcpy(r->items[1].text,"The ChAoS MVC Net is tomorrow");
  }
  *used=sizeof *r;return STNLABZ_MODULE_OK;
 }
 if(!strcmp(name,DIGIT_REASONING_SERVICE)){
  const digit_reasoning_request_t *q=req;
  digit_reasoning_response_t *r=reply;
  if(size!=sizeof *q||cap<sizeof *r||
     (strcmp(q->channel,"#stn-labz")&&strcmp(q->channel,"T1:C1"))||
     strcmp(q->sender,"PoeMei")||q->observation_count!=(ambient_fixture==1?2u:1u)||
     strcmp(q->observations[0].event_id,"event-1"))
     return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  if(!strcmp(q->message,"Digit topic discussion"))retained_full_message=1;
  memset(r,0,sizeof *r);r->version=DIGIT_REASONING_VERSION;
  r->response_eligible=q->addressed;
  r->knowledge=q->corpus_state==DIGIT_REASONING_PRESENT?
    DIGIT_REASONING_CORPUS_SUPPORTED:DIGIT_REASONING_UNKNOWN;
  r->relevance=(ambient_fixture||q->addressed)?DIGIT_REASONING_CONTEXTUAL:DIGIT_REASONING_NONE;
  r->matched_observations=ambient_fixture?(ambient_fixture==2?1:2):(q->addressed?1:0);
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
  if(!strcmp(q->term,"Digit's mission")){
   result->found=1;
   strcpy(result->id,"TEST-MISSION");
   strcpy(result->source,"fixture-mission");
   strcpy(result->text,"Digit's mission is to support authorized STN-LABZ operations.");
  }
  if(!strcasecmp(q->term,"Das is goods")||!strcasecmp(q->term,"Das is goods!")||
     !strcasecmp(q->term,"Das")||!strcasecmp(q->term,"goods")){
   result->found=1;
   strcpy(result->id,"LEARN-SOCIAL");
   strcpy(result->source,"operator");
   strcpy(result->text,"\"Das is goods\", \"des\", \"demz\", and similar playful spellings can express approval. In casual conversation, Digit may acknowledge such expressions with a short, playful response like \"Mew!\", \"Purrfect!\", or \"Digit cat approves.\"");
  }
  if(!strcasecmp(q->term,"ChAoS MVC Net")){
   selected_subject=1;
   result->found=1;
   strcpy(result->id,"TEST-NET");
   strcpy(result->source,"fixture-net");
   if(long_corpus_fixture==1)
    strcpy(result->text,"The ChAoS MVC Net is a team discussion intended to coordinate technical topics and documentation across departments. Another sentence gives lengthy background.");
   else if(long_corpus_fixture==2){
    /* [AI:GPT-6 | 2026-10-17 02:49:54 UTC] */
    strcpy(result->text,"The ChAoS MVC Net concerns company coordination");
    /* [End AI:GPT-6] */
   }
   else strcpy(result->text,"The ChAoS MVC Net concerns company coordination.");
  }
  if(!strcmp(q->term,"STN-Labz")){
   selected_subject=1;
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
/* Report the precise failed assertion without changing test expectations. */
static int test_failure(int line){
 fprintf(stderr,"Ask test assertion failed at test_ask.c:%d\n",line);
 return 1;
}
int main(void){
 stnlabz_module_qualification_result_t q;char out[512];size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t h={0};h.register_service=reg;h.unregister_service=unreg;h.invoke_service=invoke;
 if(strcmp(d->id,"ask")||d->qualify(&q)!=STNLABZ_MODULE_OK||d->start(&h)!=STNLABZ_MODULE_OK||!handler)return test_failure(__LINE__);
 if(handler("hello",6,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"hello"))return test_failure(__LINE__);
 if(handler("hi",3,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"hi"))return test_failure(__LINE__);
 if(d->version_minor!=3||d->version_patch!=9)return test_failure(__LINE__);
 if(socials_calls!=2||!strstr(out,"💚"))return test_failure(__LINE__);
 socials_available=0;
 if(handler("hello",6,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    strcmp(out,"hello"))return test_failure(__LINE__);
 socials_available=1;
 if(handler("Digit good morning",sizeof("Digit good morning"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"good morning")||!strstr(out,"💚")||strstr(out,"Corpus:"))return test_failure(__LINE__);
 if(handler("Digit good unknown",sizeof("Digit good unknown"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"clarify"))return test_failure(__LINE__);
 if(handler("what version are you?",22,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Digit 1.0.0"))return test_failure(__LINE__);
 if(handler("version",8,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Digit 1.0.0"))return test_failure(__LINE__);
 if(handler("who are you?",13,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"foundational autonomous agent")||identity_calls!=1)return test_failure(__LINE__);
 {digit_ask_context_t context={0};context.magic=DIGIT_ASK_CONTEXT_MAGIC;
  strcpy(context.question,"diagnose my last request");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"No prior request"))return test_failure(__LINE__);
  context.has_previous=1;strcpy(context.previous_request,"http-1");strcpy(context.previous_route,"/ask");strcpy(context.previous_service,"route./ask");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"DSD SCOPED REPORT"))return test_failure(__LINE__);
 }
 if(handler("what is STN-Labz?",18,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"engineering research organization")||
     !strstr(out,"fixture-corpus"))return test_failure(__LINE__);
 if(handler("what is unlisted?",18,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"no matching knowledge record"))return test_failure(__LINE__);
 if(handler("what can you tell me?",22,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"Corpus"))return test_failure(__LINE__);
 if(handler("tell me about STN-Labz",23,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"engineering research organization"))return test_failure(__LINE__);
 if(handler("research PHP",sizeof("research PHP"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"https://www.php.net/releases/")||
     !strstr(out,"Unverified source excerpt"))return test_failure(__LINE__);
 if(handler("research https://www.php.net/releases/",sizeof("research https://www.php.net/releases/"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"Retrieved source:"))return test_failure(__LINE__);
 if(handler("research unknown subject",sizeof("research unknown subject"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"not available"))return test_failure(__LINE__);
 if(handler("Hi Digit",sizeof("Hi Digit"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"Hi"))return test_failure(__LINE__);
 if(handler("digit hello",sizeof("digit hello"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    !strstr(out,"hello"))return test_failure(__LINE__);
 if(handler("digit you here?",sizeof("digit you here?"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"I'm here"))return test_failure(__LINE__);
 if(handler("digit, access your corpus",sizeof("digit, access your corpus"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"specific subject"))return test_failure(__LINE__);
 if(handler("digit picked up that report",sizeof("digit picked up that report"),out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"have not verified"))return test_failure(__LINE__);
 if(handler("delete all the records",23,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0)return test_failure(__LINE__);
 {digit_ask_context_t context={0};context.magic=DIGIT_ASK_CONTEXT_MAGIC;
  strcpy(context.transport,"irc");strcpy(context.channel,"#stn-labz");
  strcpy(context.sender,"PoeMei");
  strcpy(context.question,"I added a reasoning module for Digit, and expect a canned reply");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0)return test_failure(__LINE__);
  strcpy(context.question,"digit, what is your mission?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"support authorized STN-LABZ operations")||
     !strstr(out,"fixture-mission"))return test_failure(__LINE__);
  /* A private IRC target is the bot nick, not a #channel. The DM itself
     supplies direct address; ordinary pronouns must still resolve correctly. */
  strcpy(context.transport,"irc");strcpy(context.channel,"digit-bot");
  strcpy(context.question,"what is your mission?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"support authorized STN-LABZ operations")||
     !strstr(out,"fixture-mission"))return test_failure(__LINE__);
  strcpy(context.channel,"#stn-labz");
  strcpy(context.question,"Digit topic discussion");
  selected_subject=0;retained_full_message=0;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !selected_subject||!retained_full_message||!strstr(out,"engineering research"))return test_failure(__LINE__);
  strcpy(context.question,"Digit I fixed the problem");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"clarify"))return test_failure(__LINE__);
  strcpy(context.question,"Can Digit explain a technology that Corpus doesn't contain?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"clarify"))return test_failure(__LINE__);
  /* Shared IRC/Slack regression: learned playful example is a reply,
     never a recital of the definition or an answer to someone discussing it. */
  strcpy(context.question,"Das is goods!");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Mew!")||strstr(out,"playful spellings"))return test_failure(__LINE__);
  strcpy(context.transport,"slack");strcpy(context.channel,"T1:C1");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Mew!"))return test_failure(__LINE__);
  /* Learn from example spellings, without hardcoded vocabulary rules. */
  social_repeat_fixture=1;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Purrfect!"))return test_failure(__LINE__);
  social_repeat_fixture=0;
   strcpy(context.question,"Das is godos!"); /* adjacent swapped characters */
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Mew!"))return test_failure(__LINE__);
  strcpy(context.question,"Das is gooods!!");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"Mew!"))return test_failure(__LINE__);
  strcpy(context.question,"We were discussing Das is goods yesterday");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strstr(out,"Mew!"))return test_failure(__LINE__);

  strcpy(context.question,"is digit even seeing Das is goods!");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0||used!=1)return test_failure(__LINE__);
  strcpy(context.transport,"irc");strcpy(context.channel,"#stn-labz");
  /* Ambient question: Corpus resolves the multiword meeting subject without
     claiming Digit is prepared. No explicit bot address is present. */
  ambient_fixture=1;
  selected_subject=0;
  strcpy(context.question,"are you ready for the ChAoS MVC Net tomorrow?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !selected_subject||!strstr(out,"company coordination")||
     strstr(out,"I am ready"))return test_failure(__LINE__);
  /* Recognition uncertainty alone must not veto a separately supported
     contextual contribution. */
  recognition_unrelated=1;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"company coordination"))return test_failure(__LINE__);
  recognition_unrelated=0;
  if(!recognition_calls||!observation_lookup_calls)return test_failure(__LINE__);
  /* An existing conversational statement must not be recited back. */
  ambient_fixture=3;long_corpus_fixture=0;
  strcpy(context.question,"are you ready for the ChAoS MVC Net tomorrow?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0||used!=1)return test_failure(__LINE__);
  ambient_fixture=1;
  /* No matching prior statement: previous contextual evidence path remains. */
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"company coordination"))return test_failure(__LINE__);
  long_corpus_fixture=1;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"The ChAoS MVC Net is a team discussion.")||
     strstr(out,"Corpus:")||strlen(out)>120)return test_failure(__LINE__);
  /* [AI:GPT-6 | 2026-10-17 02:49:54 UTC]
   * Corpus propositions without a full stop remain eligible for presentation. */
  long_corpus_fixture=2;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     strcmp(out,"The ChAoS MVC Net concerns company coordination.")||
     strstr(out,"Corpus:"))return test_failure(__LINE__);
  /* [End AI:GPT-6] */
  long_corpus_fixture=0;
  ambient_fixture=2;
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"company coordination")||strstr(out,"Corpus:"))return test_failure(__LINE__);
  /* Slack receives unaddressed operations talk. A broader subject miss
     must not suppress an independently corroborated Corpus-backed topic. */
  strcpy(context.transport,"slack");strcpy(context.channel,"T1:C1");
  ambient_fixture=2;selected_subject=0;
  strcpy(context.question,"I suppose I'll get Codex ready for tomorrow's ChAoS MVC Net");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !selected_subject||!strstr(out,"company coordination")||
     strstr(out,"Corpus:"))return test_failure(__LINE__);
  strcpy(context.transport,"irc");strcpy(context.channel,"#stn-labz");
  ambient_fixture=0;
  strcpy(context.question,"are you ready for the unrelated gathering tomorrow?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0||used!=1)return test_failure(__LINE__);
  strcpy(context.question,"What is STN-Labz?");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     out[0]!=0)return test_failure(__LINE__);
  strcpy(context.question,"Digit I fixed the problem");
  strcpy(context.channel,"#other-channel");
  if(handler(&context,sizeof context,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
     !strstr(out,"clarify"))return test_failure(__LINE__);
 }
 if(handler("",1,out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK)return test_failure(__LINE__);
 if(handler("hello",6,out,2,&used,NULL)==STNLABZ_MODULE_OK)return test_failure(__LINE__);
 if(d->stop()!=STNLABZ_MODULE_OK)return test_failure(__LINE__);
 puts("Ask tests: PASS");return 0;
}
