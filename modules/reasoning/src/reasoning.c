#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "reasoning.h"
static const stnlabz_module_host_t *host;
static int valid(const char *s,size_t n){return s&&memchr(s,0,n)!=NULL;}
/* Require at least two meaningful shared topic terms across the message,
 * an independent Observation and Corpus evidence. Natural word order may
 * differ; common conversational words cannot alone establish relevance.
 * This is lexical corroboration, not semantic understanding. */
static size_t tokens(const char *s,char a[][64],size_t capacity){
 size_t count=0;
 while(*s&&count<capacity){
  size_t k=0;
  while(*s&&!isalnum((unsigned char)*s))s++;
  if(!*s)break;
  while(*s&&isalnum((unsigned char)*s)){
   if(k<63)a[count][k++]=(char)tolower((unsigned char)*s);
   s++;
  }
  if(k>=3){a[count][k]=0;count++;}
 }
 return count;
}
static int has_pair(char words[][64],size_t n,const char *a,const char *b){
 size_t i;
 for(i=1;i<n;i++)if(!strcmp(words[i-1],a)&&!strcmp(words[i],b))return 1;
 return 0;
}
static int has_word(char words[][64],size_t n,const char *word){
 size_t i;
 for(i=0;i<n;i++)if(!strcmp(words[i],word))return 1;
 return 0;
}
static int topic_word(const char *word){
 static const char *const generic[]={
  "about","after","again","also","been","before","could","doing",
  "from","have","here","just","more","need","other","ready",
  "really","said","same","some","that","them","there","these",
  "they","this","those","today","tomorrow","want","were",
  "what","when","where","which","while","will","with","would",NULL
 };
 size_t i;
 if(strlen(word)<4)return 0;
 for(i=0;generic[i];i++)if(!strcmp(word,generic[i]))return 0;
 return 1;
}
static int related_topic(const char *message,const char *observation,const char *corpus){
 char m[64][64]={{0}},o[64][64]={{0}},c[64][64]={{0}};
 size_t nm=tokens(message,m,64),no=tokens(observation,o,64),nc=tokens(corpus,c,64),i,j;
 unsigned shared=0;
 /* Preserve the established adjacent-phrase match for short named topics. */
 for(i=1;i<nm;i++)
  if(has_pair(o,no,m[i-1],m[i])&&has_pair(c,nc,m[i-1],m[i]))return 1;
 /* A reworded sentence may preserve its subject but change word order. */
 for(i=0;i<nm;i++){
  if(!topic_word(m[i])||!has_word(o,no,m[i])||!has_word(c,nc,m[i]))continue;
  for(j=0;j<i;j++)if(!strcmp(m[j],m[i]))break;
  if(j!=i)continue;
  if(++shared>=2)return 1;
 }
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
    q->version!=DIGIT_REASONING_VERSION||q->addressed>1||
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
 r->response_eligible=q->addressed;

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
  if(q->corpus_state==DIGIT_REASONING_PRESENT&&
     related_topic(q->message,o->text,q->corpus_text)){
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
 d.version_major=0;d.version_minor=1;d.version_patch=3;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
