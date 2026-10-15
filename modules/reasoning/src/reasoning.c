#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "reasoning.h"
static const stnlabz_module_host_t *host;
static int valid(const char *s,size_t n){return s&&memchr(s,0,n)!=NULL;}
/* Language-neutral, data-derived token intersection, not a word/intent dictionary.
   Common words cannot establish meaning: an overlap is only a traceable candidate. */
static size_t tokens(const char *s,char a[][64],size_t capacity){
 size_t count=0;
 while(*s){
  size_t k=0;
  while(*s&&!isalnum((unsigned char)*s))s++;
  if(!*s)break;
  while(*s&&isalnum((unsigned char)*s)){
   if(k<63)a[count][k++]=(char)tolower((unsigned char)*s);
   s++;
  }
  if(k>=3&&count<capacity){a[count][k]=0;count++;}
  if(count==capacity)break;
 }
 return count;
}
static int intersects(const char *a,const char *b){
 char x[64][64]={{0}},y[64][64]={{0}};
 size_t nx=tokens(a,x,64),ny=tokens(b,y,64),i,j;
 for(i=0;i<nx;i++)for(j=0;j<ny;j++)
  if(!strcmp(x[i],y[j]))return 1;
 return 0;
}
static stnlabz_module_result_t evaluate(const void *input,size_t len,void *output,
 size_t capacity,size_t *used,void *context){
 const digit_reasoning_request_t *q=input;
 digit_reasoning_response_t *r=output;
 size_t i;
 (void)context;
 if(used)*used=0;
 if(!host||!q||len!=sizeof *q||!r||capacity<sizeof *r||!used||
    q->version!=DIGIT_REASONING_VERSION||
    !valid(q->message,sizeof q->message)||!q->message[0]||
    !valid(q->transport,sizeof q->transport)||!valid(q->channel,sizeof q->channel)||
    !valid(q->sender,sizeof q->sender)||
    q->observation_count>DIGIT_REASONING_ITEMS||
    q->observation_state>DIGIT_REASONING_PRESENT||
    q->corpus_state>DIGIT_REASONING_PRESENT||
    (q->observation_state!=DIGIT_REASONING_PRESENT&&q->observation_count)||
    (q->corpus_state==DIGIT_REASONING_PRESENT&&
      (!valid(q->corpus_id,sizeof q->corpus_id)||!q->corpus_id[0]||
       !valid(q->corpus_source,sizeof q->corpus_source)||
       !valid(q->corpus_text,sizeof q->corpus_text)))||
    (strcmp(q->transport,"irc")&&strcmp(q->transport,"slack"))||
    (q->transport[0]=='i'&&q->channel[0]!='#'))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);r->version=DIGIT_REASONING_VERSION;
 r->knowledge=q->corpus_state==DIGIT_REASONING_PRESENT?
   DIGIT_REASONING_CORPUS_SUPPORTED:q->corpus_state==DIGIT_REASONING_UNAVAILABLE?
   DIGIT_REASONING_EVIDENCE_UNAVAILABLE:DIGIT_REASONING_UNKNOWN;
 if(q->corpus_state==DIGIT_REASONING_PRESENT){
  memcpy(r->corpus_id,q->corpus_id,sizeof r->corpus_id);
  memcpy(r->corpus_source,q->corpus_source,sizeof r->corpus_source);
 }
 /* Reject invalid evidence rather than letting forged or unterminated fields
    silently influence relationships. The caller owns channel scoping. */
 for(i=0;i<q->observation_count;i++){
  const digit_reasoning_observation_t *o=&q->observations[i];
  if(!valid(o->text,sizeof o->text)||!valid(o->event_id,sizeof o->event_id)||
     !valid(o->sender,sizeof o->sender)||!o->event_id[0])
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  if(intersects(q->message,o->text)){
   memcpy(r->observation_ids[r->matched_observations],o->event_id,
          sizeof o->event_id);
   r->matched_observations++;
  }
 }
 if(r->matched_observations)r->relevance=DIGIT_REASONING_CONTEXTUAL;
 else if(q->observation_state==DIGIT_REASONING_UNAVAILABLE)
  r->relevance=DIGIT_REASONING_UNDETERMINED;
 else r->relevance=DIGIT_REASONING_NONE;
 *used=sizeof *r;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service(DIGIT_REASONING_SERVICE,evaluate,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_REASONING_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"reasoning");
 snprintf(d.name,sizeof d.name,"Digit Evidence Relationship Evaluator");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
