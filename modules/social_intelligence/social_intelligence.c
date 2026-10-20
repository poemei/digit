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
static int social(const digit_interpretation_result_t *r){
 unsigned i;int found=0;
 if(r->kind==DIGIT_UTTERANCE_QUESTION)return 0;
 for(i=0;i<r->term_count&&i<DIGIT_INTERPRETATION_TERMS_MAX;i++){
  const char *w=r->terms[i];
  if(!strcmp(w,"mrrp")||!strcmp(w,"meow")||
     !strcmp(w,"purr")||!strcmp(w,"purrs")||
     !strcmp(w,"puurrs")||!strcmp(w,"paws")||
     !strcmp(w,"lickies")||!strcmp(w,"scratches")||
     !strcmp(w,"cuddles"))found=1;
 }
 return found;
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
 if(q->account[0]&&preference(q->text,desired)){
  snprintf(channel,sizeof channel,"%s",q->channel);
  snprintf(language,sizeof language,"%s",desired);
  last_reply=0;turns=0;
  out->handled=1;
  snprintf(out->response,sizeof out->response,
    "I hear you. I'll use the social expressions I know here; "
    "that doesn't establish fluency in %s.",desired);
 }else if(social(&q->interpretation)){
  int reply=!q->passive;
  out->handled=1;
  if(!reply&&language[0]&&!strcmp(channel,q->channel)){
   if(++turns>=2&&(last_reply==0||
      (time(NULL)!=(time_t)-1&&time(NULL)-last_reply>=60)))
      reply=1;
  }
  if(reply){
   digit_socials_request_t sr={{0}};
   digit_socials_response_t result={{0}};
   size_t n=0;
   strcpy(sr.category,"playful");
   if(api->invoke_service(DIGIT_SOCIALS_SERVICE,&sr,sizeof sr,
      &result,sizeof result,&n)==STNLABZ_MODULE_OK&&
      n==sizeof result&&memchr(result.expression,0,sizeof result.expression)){
    snprintf(out->response,sizeof out->response,"%s",result.expression);
    turns=0;last_reply=time(NULL);
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
 api=saved;
 channel[0]=language[0]=0;last_reply=0;turns=0;
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
 api=NULL;channel[0]=language[0]=0;turns=0;last_reply=0;
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="social_intelligence",.name="Social Intelligence",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
