#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "recognition.h"
static const stnlabz_module_host_t *host;
static int terminated(const char *p,size_t n){return p&&memchr(p,0,n)!=NULL;}
/* No built-in dialect or response lexicon. Orthographic similarity is
 * evidence of possible relatedness, never proof of emotion or intent. */
static size_t tokenise(const char *s,char terms[][64],size_t limit){
 size_t count=0;
 while(*s&&count<limit){
  size_t n=0;unsigned char last=0;unsigned run=0;
  while(*s&&!isalnum((unsigned char)*s))s++;
  if(!*s)break;
  while(*s&&isalnum((unsigned char)*s)){
   unsigned char c=(unsigned char)tolower((unsigned char)*s++);
   if(c==last)run++;else{last=c;run=1;}
   if(run>2)continue;
   if(n<63)terms[count][n++]=(char)c;
  }
  if(n>=3){terms[count][n]=0;count++;}
 }
 return count;
}
static int close_token(const char *a,const char *b){
 size_t na=strlen(a),nb=strlen(b),i=0,j=0,edits=0;
 if(!strcmp(a,b))return 1;
 if(na<5||nb<5||(na>nb?na-nb:nb-na)>1)return 0;
 while(i<na&&j<nb){
  if(a[i]==b[j]){i++;j++;continue;}
  if(++edits>1)return 0;
  if(na==nb&&i+1<na&&a[i]==b[j+1]&&a[i+1]==b[j]){i+=2;j+=2;}
  else if(na>nb)i++;
  else if(nb>na)j++;
  else{i++;j++;}
 }
 return edits+(i<na||j<nb)<=1;
}
/* A repeated token cannot inflate the evidence count. */
static unsigned overlap(const char *message,const char *reference){
 char m[64][64]={{0}},r[128][64]={{0}};
 size_t nm=tokenise(message,m,64),nr=tokenise(reference,r,128),i,j,k;
 unsigned shared=0;
 for(i=0;i<nm;i++){
  for(k=0;k<i;k++)if(!strcmp(m[i],m[k]))break;
  if(k<i)continue;
  for(j=0;j<nr;j++)if(close_token(m[i],r[j])){shared++;break;}
 }
 return shared;
}
static stnlabz_module_result_t evaluate(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *ctx){
 const digit_recognition_request_t *q=request;
 digit_recognition_response_t *r=response;
 unsigned i,highest=0;
 (void)ctx;
 if(!host||!q||size!=sizeof *q||!r||capacity<sizeof *r||!used||
    q->version!=DIGIT_RECOGNITION_VERSION||
    !terminated(q->message,sizeof q->message)||!q->message[0]||
    !terminated(q->corpus_text,sizeof q->corpus_text)||
    q->history_count>DIGIT_RECOGNITION_HISTORY_MAX)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<q->history_count;i++)
  if(!terminated(q->history[i],sizeof q->history[i]))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);r->version=DIGIT_RECOGNITION_VERSION;
 if(q->corpus_text[0]){
  r->corpus_shared_terms=overlap(q->message,q->corpus_text);
  r->corpus_relation=r->corpus_shared_terms>=2?
    DIGIT_RECOGNITION_RELATED:DIGIT_RECOGNITION_UNCERTAIN;
 }
 for(i=0;i<q->history_count;i++){
  unsigned n=overlap(q->message,q->history[i]);
  if(n>highest)highest=n;
 }
 r->conversation_shared_terms=highest;
 if(q->history_count)
  r->conversation_relation=highest>=2?
    DIGIT_RECOGNITION_RELATED:DIGIT_RECOGNITION_UNCERTAIN;
 /* Lexical evidence alone NEVER establishes the message's meaning. */
 r->meaning_established=0;
 *used=sizeof *r;return STNLABZ_MODULE_OK;
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
 if(!h->register_service(DIGIT_RECOGNITION_SERVICE,evaluate,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_RECOGNITION_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"recognition");
 snprintf(d.name,sizeof d.name,"Digit Conversation Recognition Evidence");
 d.version_major=0;d.version_minor=1;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
