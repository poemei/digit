/* Digit Social Intelligence: bounded, channel-scoped social dispatch.
 * No factual claims, role attribution, or language fluency inferred. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "module.h"
#include "social_intelligence.h"
#include "../socials/socials.h"
static const stnlabz_module_host_t *api;
static char channel[128],language[48];
static time_t last_reply;
static unsigned turns;
static char observed_channel[128];
static char quiet_channel[128];
static int preference(const char *text,char word[48]){
 const char *p=text,*start;size_t n,i;
 const char *prefix="digit use ",*suffix=" when you speak to ";
 while(isspace((unsigned char)*p))p++;
 for(i=0;prefix[i];i++)if(!p[i]||tolower((unsigned char)p[i])!=prefix[i])return 0;
 p+=i;start=p;
 while(isalnum((unsigned char)*p)||*p=='_'||*p=='-')p++;
 n=(size_t)(p-start);
 if(!n||n>=48)return 0;
 for(i=0;suffix[i];i++)if(!p[i]||tolower((unsigned char)p[i])!=suffix[i])return 0;
 p+=i;
 if(p[0]=='m'&&p[1]=='e')p+=2;
 else if(p[0]=='u'&&p[1]=='s')p+=2;
 else return 0;
 while(isspace((unsigned char)*p))p++;
 if(*p=='.'||*p=='!')p++;
 if(*p)return 0;
 memcpy(word,start,n);word[n]=0;return 1;
}
static int emoticon(const char *t){
 const unsigned char *p=(const unsigned char *)t;
 size_t n=strlen(t);int mark=0;
 if(!n||n>24)return 0;
 while(*p){
  unsigned char c=*p++;
  if(c=='?'||c=='!')return 0;
  if(c==':'||c==';'||c=='<'||c=='^'||c==')'||c=='('||
     c=='='||c=='*'||c>=128)mark=1;
  else if(c!='3'&&c!='D'&&c!='d'&&c!='x'&&c!='X'&&
          c!='-'&&c!='.'&&c!=' '&&c!=9)return 0;
 }
 return mark;
}
/* Score conversational signals, not a lookup table of replies.
 * Textual play and affectionate actions are weak signals independently;
 * multiple turns are required before unsolicited participation. */
static int prefix(const char *word,const char *stem){
 size_t n=strlen(stem);
 return !strncmp(word,stem,n);
}
static int social(const digit_interpretation_result_t *r){
 unsigned i;int markers=0;
 if(r->kind==DIGIT_UTTERANCE_QUESTION||
    r->kind==DIGIT_UTTERANCE_UNRESOLVED)return 0;
 for(i=0;i<r->term_count&&i<DIGIT_INTERPRETATION_TERMS_MAX;i++){
  const char *w=r->terms[i];
  /* Morphological families permit informal spellings and inflection.
   * These are social cues, not evidence of intent or consent. */
  if(prefix(w,"purr")||prefix(w,"puurr")||prefix(w,"meow")||
     prefix(w,"mrrp")||prefix(w,"lick")||prefix(w,"paws")||
     prefix(w,"bump")||prefix(w,"tackl")||prefix(w,"cuddl")||
     prefix(w,"gigg")||prefix(w,"nuzzl")||prefix(w,"scritch")||
     prefix(w,"kitten")||prefix(w,"kitty")||prefix(w,"bowl")||
     prefix(w,"hug")||prefix(w,"snugg")||prefix(w,"queen")||
     prefix(w,"groom")||prefix(w,"pet")||prefix(w,"scratches"))
     markers++;
 }
 return markers>0;
}
/* A quiet request is only actionable when addressed to Digit.
 * The IRC transport removes a leading address; passive channel messages
 * retain their text and therefore require an explicit Digit reference. */
static int has_word(const char *text,const char *word){
 const char *p=text;size_t n=strlen(word);
 while(*p){
  size_t i=0;
  while(*p&&!isalnum((unsigned char)*p))p++;
  if(!*p)break;
  while(isalnum((unsigned char)p[i])||p[i]=='_')i++;
  if(i==n){
   size_t j;
   for(j=0;j<n;j++)
    if(tolower((unsigned char)p[j])!=word[j])break;
   if(j==n)return 1;
  }
  p+=i;
 }
 return 0;
}
static int addressed(const digit_social_intelligence_request_t *q){
 return !q->passive||has_word(q->text,"digit")||
                         has_word(q->text,"digicat");
}
static int quiet_request(const digit_social_intelligence_request_t *q){
 if(!addressed(q))return 0;
 return has_word(q->text,"chill")||has_word(q->text,"quiet")||
        has_word(q->text,"shush")||
        (has_word(q->text,"stop")&&
         (has_word(q->text,"talking")||has_word(q->text,"speaking")));
}
static int resume_request(const digit_social_intelligence_request_t *q){
 if(!addressed(q))return 0;
 return (has_word(q->text,"resume")||
         (has_word(q->text,"join")&&has_word(q->text,"in"))) &&
        !has_word(q->text,"not")&&!has_word(q->text,"dont");
}
static stnlabz_module_result_t process(const void *request,size_t bytes,
 void *response,size_t capacity,size_t *used,void *ctx){
 const digit_social_intelligence_request_t *q=request;
 digit_social_intelligence_result_t *out=response;
 char desired[48]={0};
 (void)ctx;
 if(used)*used=0;
 if(!api||!api->invoke_service)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!q||bytes!=sizeof *q||!out||capacity<sizeof *out||!used||
    !memchr(q->channel,0,sizeof q->channel)||
    !memchr(q->account,0,sizeof q->account)||
    !memchr(q->text,0,sizeof q->text)||
    q->interpretation.term_count>DIGIT_INTERPRETATION_TERMS_MAX||
    q->interpretation.kind>DIGIT_UTTERANCE_QUESTION)
     return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);
 if(q->account[0]&&quiet_request(q)){
  snprintf(quiet_channel,sizeof quiet_channel,"%s",q->channel);
  turns=0;out->handled=1;
  snprintf(out->response,sizeof out->response,"Okay, I'll be quiet here.");
 }else if(q->account[0]&&resume_request(q)){
  if(!strcmp(quiet_channel,q->channel))quiet_channel[0]=0;
  turns=0;out->handled=1;
  snprintf(out->response,sizeof out->response,"Okay, I can join in again.");
 }else if(q->account[0]&&preference(q->text,desired)){
  snprintf(channel,sizeof channel,"%s",q->channel);
  snprintf(language,sizeof language,"%s",desired);
  last_reply=0;turns=0;
  snprintf(observed_channel,sizeof observed_channel,"%s",q->channel);
  out->handled=1;
  snprintf(out->response,sizeof out->response,
    "I hear you. I'll use the social expressions I know here; "
    "that doesn't establish fluency in %s.",desired);
 }else{
  const int cue=social(&q->interpretation)||emoticon(q->text);
  const time_t now=time(NULL);
  int reply=0;
  /* Channel traffic is conversational context even when individual turns
   * are not social cues. Keep DM interactions immediate, but allow a
   * relevant social cue after other channel participants have been talking.
   * Do not require an ephemeral preference to be repeated after reload. */
  if(q->passive){
   if(!strcmp(quiet_channel,q->channel)){
    if(cue)out->handled=1;
    *used=sizeof *out;return STNLABZ_MODULE_OK;
   }
   if(strcmp(observed_channel,q->channel)){
    snprintf(observed_channel,sizeof observed_channel,"%s",q->channel);
    turns=0;
   }
   if(q->interpretation.kind==DIGIT_UTTERANCE_STATEMENT&&
      q->text[0]&&turns<8)turns++;
   if(cue&&turns>=3&&
      (last_reply==0||(now!=(time_t)-1&&now>=last_reply&&
       now-last_reply>=60)))reply=1;
  }else if(cue)reply=1;
  if(cue){
   digit_socials_request_t sr={{0}};
   digit_socials_response_t result={{0}};
   size_t n=0;
   out->handled=1;
   if(reply){
    strcpy(sr.category,emoticon(q->text)?"cats":"playful");
    if(api->invoke_service(DIGIT_SOCIALS_SERVICE,&sr,sizeof sr,
       &result,sizeof result,&n)==STNLABZ_MODULE_OK&&
       n==sizeof result&&memchr(result.expression,0,sizeof result.expression)){
     snprintf(out->response,sizeof out->response,"%s",result.expression);
     if(q->passive){turns=0;last_reply=now;}
    }
   }
  }
 }

 *used=sizeof *out;return STNLABZ_MODULE_OK;
}
static void check(stnlabz_module_qualification_result_t *q,int ok,int negative){
 q->tests_executed++;
 if(ok)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(ok)q->negative_test_passed=1;}
}
static stnlabz_module_result_t fake(const char *name,const void *req,size_t size,
 void *res,size_t cap,size_t *used){
 digit_socials_response_t v={{0}};
 (void)req;(void)size;
 if(strcmp(name,DIGIT_SOCIALS_SERVICE)||!res||cap<sizeof v||!used)
  return STNLABZ_MODULE_ERR_NOT_FOUND;
 strcpy(v.expression,"=^.^=");
 memcpy(res,&v,sizeof v);*used=sizeof v;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 const stnlabz_module_host_t stub={.invoke_service=fake};
 const stnlabz_module_host_t *saved=api;
 digit_social_intelligence_request_t in={0};
 digit_social_intelligence_result_t out={0};
 size_t used=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 api=&stub;
 strcpy(in.account,"trusted");strcpy(in.channel,"#test");
 strcpy(in.text,"digit use kitten when you speak to us");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  used==sizeof out&&out.handled&&strstr(out.response,"kitten"),0);
 in.text[0]=0;strcpy(in.text,"mrrp");
 in.interpretation.kind=DIGIT_UTTERANCE_STATEMENT;
 in.interpretation.term_count=1;strcpy(in.interpretation.terms[0],"mrrp");
 in.passive=1;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  out.handled&&!out.response[0],0);
 /* Ordinary channel discussion contributes to activity before a cue. */
 strcpy(in.text,"ordinary chat");in.interpretation.term_count=2;
 strcpy(in.interpretation.terms[0],"ordinary");
 strcpy(in.interpretation.terms[1],"chat");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  !out.handled,0);
 strcpy(in.text,"mrrp");in.interpretation.term_count=1;
 strcpy(in.interpretation.terms[0],"mrrp");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  out.handled&&!strcmp(out.response,"=^.^="),0);
 strcpy(in.channel,"#other");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  out.handled&&!out.response[0],1);
 in.passive=0;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  !strcmp(out.response,"=^.^="),0);
 in.interpretation.kind=DIGIT_UTTERANCE_QUESTION;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
  !out.handled,1);
 check(q,process(&in,sizeof in,NULL,sizeof out,&used,NULL)==
  STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,process(&in,sizeof in-1,&out,sizeof out,&used,NULL)==
  STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,process(NULL,sizeof in,&out,sizeof out,&used,NULL)==
  STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 in.interpretation.term_count=DIGIT_INTERPRETATION_TERMS_MAX+1;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==
  STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 /* Interface bounds and negative controls, all executed. */
 in.interpretation.term_count=0;in.interpretation.kind=DIGIT_UTTERANCE_STATEMENT;
 strcpy(in.text,"ordinary statement");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,0);
 strcpy(in.text,":)");in.passive=0;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!strcmp(out.response,"=^.^="),0);
 in.passive=1;
 strcpy(in.channel,"#quiet");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!out.response[0],0);
 in.passive=0;strcpy(in.text,"");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,0);
 memset(in.text,'x',sizeof in.text);
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 memset(in.text,0,sizeof in.text);
 memset(in.channel,'x',sizeof in.channel);
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 memset(in.channel,0,sizeof in.channel);
 memset(in.account,'x',sizeof in.account);
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 memset(in.account,0,sizeof in.account);
 check(q,process(&in,sizeof in,&out,sizeof out-1,&used,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,process(&in,sizeof in,&out,sizeof out,NULL,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 in.interpretation.kind=DIGIT_UTTERANCE_QUESTION+1;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==
   STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 /* Realistic multi-speaker social-turn qualification. All observations
  * use the same channel, without assuming the speakers are family. */
 channel[0]=language[0]=observed_channel[0]=quiet_channel[0]=0;last_reply=0;turns=0;
 memset(&in,0,sizeof in);
 strcpy(in.account,"speaker-a");strcpy(in.channel,"#social");
 strcpy(in.text,"digit use kitten when you speak to us");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&out.response[0],0);
 in.passive=1;
 strcpy(in.text,"head bumpies you");
 in.interpretation.kind=DIGIT_UTTERANCE_STATEMENT;
 in.interpretation.term_count=3;
 strcpy(in.interpretation.terms[0],"head");
 strcpy(in.interpretation.terms[1],"bumpies");
 strcpy(in.interpretation.terms[2],"you");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!out.response[0],0);
 strcpy(in.account,"speaker-b");
 strcpy(in.text,"yes :)");
 in.interpretation.term_count=1;strcpy(in.interpretation.terms[0],"yes");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,0);
 in.interpretation.term_count=3;
 strcpy(in.text,"runs and tackles yous");
 strcpy(in.interpretation.terms[0],"runs");
 strcpy(in.interpretation.terms[1],"tackles");
 strcpy(in.interpretation.terms[2],"yous");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!strcmp(out.response,"=^.^="),0);
 strcpy(in.text,"What is the server status?");
 in.interpretation.kind=DIGIT_UTTERANCE_QUESTION;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,1);
 in.interpretation.kind=DIGIT_UTTERANCE_STATEMENT;
 strcpy(in.text,"regular discussion");
 strcpy(in.interpretation.terms[0],"regular");
 strcpy(in.interpretation.terms[1],"discussion");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,1);
 /* Directly addressed quiet request and explicit re-entry. */
 memset(&in,0,sizeof in);
 strcpy(in.channel,"#social");strcpy(in.account,"speaker-b");
 strcpy(in.text,"and digit chill da phuk out k?");
 in.passive=1;in.interpretation.kind=DIGIT_UTTERANCE_QUESTION;
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&strstr(out.response,"quiet"),0);
 strcpy(in.text,"puurrs");
 in.interpretation.kind=DIGIT_UTTERANCE_STATEMENT;in.interpretation.term_count=1;
 strcpy(in.interpretation.terms[0],"puurrs");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!out.response[0],1);
 strcpy(in.text,"digit join in");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&strstr(out.response,"join in again"),0);
 strcpy(in.text,"puurrs");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   out.handled&&!out.response[0],0);
 strcpy(in.text,"chill out");
 check(q,process(&in,sizeof in,&out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK&&
   !out.handled,1);
 api=saved;
 channel[0]=language[0]=quiet_channel[0]=observed_channel[0]=0;last_reply=0;turns=0;
 return q->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&!q->tests_failed&&
  q->negative_test_executed&&q->negative_test_passed?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 if(api||!host||!host->register_service||!host->unregister_service||
    !host->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_service(DIGIT_SOCIAL_INTELLIGENCE_SERVICE,process,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 api=host;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!api->unregister_service(DIGIT_SOCIAL_INTELLIGENCE_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 api=NULL;channel[0]=language[0]=observed_channel[0]=0;turns=0;last_reply=0;
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="social_intelligence",.name="Social Intelligence",
 .version_major=0,.version_minor=1,.version_patch=3,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
