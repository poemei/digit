/* Read-only learning-material preview, ABI 1.5.
 * Observations are claims about speech; Corpus excerpts retain their
 * verification metadata. No examples are approved, queued, or trained.
 * This module does not fabricate response pairs or read SQLite directly. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include "unistd.h"
#include "../sqlite3/gru_feed.h"
#include "../sqlite3/context.h"
#include "../gru_bridge/gru_bridge.h"
/* [End AI:GPT-6] */
#include "module.h"
#include "../observation/observation.h"
#include "../sqlite3/corpus_search.h"
#include "gru_learning.h"

static const stnlabz_module_host_t *host;
static int terminated(const char *value,size_t cap){
 return value&&memchr(value,0,cap)!=NULL;
}
static int validate(const digit_gru_learning_preview_request_t *request){
 return request&&
  terminated(request->channel,sizeof request->channel)&&
  terminated(request->query,sizeof request->query)&&
  (request->channel[0]||request->query[0]);
}
static stnlabz_module_result_t preview(const void *raw,size_t request_size,
 void *response,size_t response_capacity,size_t *response_used,void *context){
 const digit_gru_learning_preview_request_t *request=raw;
 digit_gru_learning_preview_result_t *out=response;
 stnlabz_module_result_t rc;
 size_t used=0;
 (void)context;
 if(response_used)*response_used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!raw||request_size!=sizeof *request||!validate(request)||
    !out||response_capacity<sizeof *out||!response_used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);
 if(request->channel[0]){
  digit_observation_recent_query_t query={0};
  digit_observation_recent_result_t recent={0};
  unsigned i;
  memcpy(query.channel,request->channel,sizeof query.channel);
  rc=host->invoke_service(DIGIT_OBSERVATION_RECENT_SERVICE,
      &query,sizeof query,&recent,sizeof recent,&used);
  if(rc==STNLABZ_MODULE_OK){
   if(used!=sizeof recent||recent.count>DIGIT_OBSERVATION_RECENT_MAX)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   out->observations_available=1;
   out->observations_count=recent.count;
   for(i=0;i<recent.count;i++){
    if(!terminated(recent.items[i].sender,sizeof recent.items[i].sender)||
       !terminated(recent.items[i].text,sizeof recent.items[i].text))
      return STNLABZ_MODULE_ERR_INVALID_STATE;
    memcpy(out->observations[i].sender,recent.items[i].sender,
           sizeof recent.items[i].sender);
    memcpy(out->observations[i].text,recent.items[i].text,
           sizeof recent.items[i].text);
   }
  }else if(rc!=STNLABZ_MODULE_ERR_NOT_FOUND)return rc;
 }
/* [AI:GPT-6 | 2026-10-20 UTC] */
 /* Context carries event provenance; do not interpret utterances as facts. */
 if(request->channel[0]){
  digit_context_recent_query_t cq={0};
  digit_context_recent_result_t contexts={0};
  size_t context_used=0;
  unsigned i;
  memcpy(cq.channel,request->channel,sizeof cq.channel);
  rc=host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE,
      &cq,sizeof cq,&contexts,sizeof contexts,&context_used);
  if(rc==STNLABZ_MODULE_OK){
   if(context_used!=sizeof contexts||contexts.count>DIGIT_CONTEXT_MAX)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   for(i=0;i<contexts.count;i++){
    if(!terminated(contexts.items[i].source_id,sizeof contexts.items[i].source_id)||
       !terminated(contexts.items[i].context_type,sizeof contexts.items[i].context_type)||
       !terminated(contexts.items[i].value,sizeof contexts.items[i].value))
      return STNLABZ_MODULE_ERR_INVALID_STATE;
    memcpy(out->context[i].source_id,contexts.items[i].source_id,
           sizeof contexts.items[i].source_id);
    memcpy(out->context[i].context_type,contexts.items[i].context_type,
           sizeof contexts.items[i].context_type);
    memcpy(out->context[i].value,contexts.items[i].value,
           sizeof contexts.items[i].value);
   }
   out->context_available=1;
   out->context_count=contexts.count;
  }else if(rc!=STNLABZ_MODULE_ERR_NOT_FOUND)return rc;
 }
/* [End AI:GPT-6] */
 if(request->query[0]){
  digit_corpus_query_t query={0};
  digit_corpus_result_t found={0};
  unsigned i;
  memcpy(query.query,request->query,sizeof query.query);
  used=0;
  rc=host->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,
      &query,sizeof query,&found,sizeof found,&used);
  if(rc==STNLABZ_MODULE_OK){
   if(used!=sizeof found||found.count>DIGIT_CORPUS_RESULTS_MAX)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   out->corpus_available=1;out->corpus_count=found.count;
   for(i=0;i<found.count;i++){
    const digit_corpus_hit_t *hit=&found.hits[i];
    if(!terminated(hit->category,sizeof hit->category)||
       !terminated(hit->verification_status,sizeof hit->verification_status)||
       !terminated(hit->excerpt,sizeof hit->excerpt))
      return STNLABZ_MODULE_ERR_INVALID_STATE;
    out->corpus[i].id=hit->id;
    memcpy(out->corpus[i].category,hit->category,sizeof hit->category);
    memcpy(out->corpus[i].verification_status,hit->verification_status,
           sizeof hit->verification_status);
    memcpy(out->corpus[i].excerpt,hit->excerpt,sizeof hit->excerpt);
   }
  }else if(rc!=STNLABZ_MODULE_ERR_NOT_FOUND)return rc;
 }
 *response_used=sizeof *out;
 return STNLABZ_MODULE_OK;
}
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
/* Verified IRC event plus an explicitly approved reviewed response.
 * No fabricated input, source, conversation pairing or auto-submission. */
static int submit_valid(const digit_gru_learning_submit_request_t *q){
 return q&&q->approved==1&&terminated(q->channel,sizeof q->channel)&&
  !strcmp(q->channel,"#stn-labz")&&
  terminated(q->event_id,sizeof q->event_id)&&q->event_id[0]&&
  terminated(q->response,sizeof q->response)&&q->response[0];
}
static stnlabz_module_result_t submit(const void *raw,size_t bytes,
 void *response,size_t capacity,size_t *used,void *ctx){
 const digit_gru_learning_submit_request_t *q=raw;
 digit_context_recent_query_t query={0};
 digit_context_recent_result_t recent={0};
 digit_gru_learn_request_t learn={0};
 digit_gru_learn_result_t outcome={0};
 stnlabz_module_result_t rc;
 size_t received=0;
 unsigned i;int found=0,n;
 (void)ctx;
 if(used)*used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!raw||bytes!=sizeof *q||!submit_valid(q)||!response||
    capacity<sizeof outcome||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(query.channel,q->channel,sizeof query.channel);
 rc=host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE,
  &query,sizeof query,&recent,sizeof recent,&received);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(received!=sizeof recent||recent.count>DIGIT_CONTEXT_MAX)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<recent.count;i++){
  const digit_context_item_t *item=&recent.items[i];
  if(!terminated(item->source_id,sizeof item->source_id)||
     !terminated(item->context_type,sizeof item->context_type)||
     !terminated(item->value,sizeof item->value))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(strcmp(item->source_id,q->event_id)||
     strcmp(item->context_type,"utterance.observed"))continue;
  if(!item->value[0]||found)return STNLABZ_MODULE_ERR_INVALID_STATE;
  memcpy(learn.input,item->value,sizeof item->value);found=1;
 }
 if(!found)return STNLABZ_MODULE_ERR_NOT_FOUND;
 memcpy(learn.response,q->response,sizeof q->response);
 n=snprintf(learn.source,sizeof learn.source,"irc:%s:%s",q->channel,q->event_id);
 if(n<0||(size_t)n>=sizeof learn.source)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 learn.approved=1;received=0;
 rc=host->invoke_service(DIGIT_GRU_LEARN_SERVICE,
  &learn,sizeof learn,&outcome,sizeof outcome,&received);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(received!=sizeof outcome)return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,&outcome,sizeof outcome);*used=sizeof outcome;
 return STNLABZ_MODULE_OK;
}
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
/* Operator-authorized automatic examples are restricted to a real IRC
 * observation paired with Digit's verified, Corpus-attributed response.
 * A GRU acceptance is a review-queue receipt, not model training. */
static int exchange_valid(const digit_gru_learning_exchange_request_t *q){
 char citation[64];
 int n;
 if(!q||strcmp(q->channel,"#stn-labz")||
    !terminated(q->input,sizeof q->input)||!q->input[0]||
    !terminated(q->response,sizeof q->response)||!q->response[0]||
    (strcmp(q->verification_status,"verified")&&
     strcmp(q->verification_status,"operator-verified"))||
    q->corpus_id<=0)return 0;
 n=snprintf(citation,sizeof citation,"Corpus #%lld",(long long)q->corpus_id);
 return n>0&&(size_t)n<sizeof citation&&strstr(q->response,citation)!=NULL;
}
static int input_matches_observation(const char *saved,const char *input){
 size_t a,b;
 if(!saved||!input)return 0;
 if(!strcmp(saved,input))return 1;
 a=strlen(saved);b=strlen(input);
 /* IRC may strip "Digit: " from addressed messages when dispatching. */
 return a>b&&a-b<=72&&!strcmp(saved+a-b,input)&&
   (saved[a-b-1]==' '||saved[a-b-1]==':'||saved[a-b-1]==',');
}
static stnlabz_module_result_t exchange(const void *raw,size_t bytes,
 void *response,size_t cap,size_t *used,void *context){
 const digit_gru_learning_exchange_request_t *q=raw;
 digit_context_recent_query_t query={0};
 digit_context_recent_result_t recent={0};
 digit_gru_learn_request_t request={0};
 digit_gru_learn_result_t result={0};
 size_t size=0;
 stnlabz_module_result_t rc;
 const char *source=NULL;
 unsigned i;
 int count=0,n;
 (void)context;
 if(used)*used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!raw||bytes!=sizeof *q||!exchange_valid(q)||
    !response||cap<sizeof result||!used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(query.channel,q->channel,sizeof query.channel);
 rc=host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE,
     &query,sizeof query,&recent,sizeof recent,&size);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(size!=sizeof recent||recent.count>DIGIT_CONTEXT_MAX)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<recent.count;i++){
  digit_context_item_t *item=&recent.items[i];
  if(!terminated(item->source_id,sizeof item->source_id)||
     !terminated(item->context_type,sizeof item->context_type)||
     !terminated(item->value,sizeof item->value))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(strcmp(item->context_type,"utterance.observed")||
     !input_matches_observation(item->value,q->input))continue;
  source=item->source_id;
  if(!source[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
  /* Prefer the latest matching persisted IRC event. */
  count++;
 }
 if(!count)return STNLABZ_MODULE_ERR_NOT_FOUND;
 memcpy(request.input,q->input,sizeof q->input);
 memcpy(request.response,q->response,sizeof q->response);
 n=snprintf(request.source,sizeof request.source,"irc:%s",source);
 if(n<=0||(size_t)n>=sizeof request.source)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 /* This flag authorizes API review submission under the operator-approved
  * automatic, verified-response policy, not automatic model training. */
 request.approved=1;
 size=0;
 rc=host->invoke_service(DIGIT_GRU_LEARN_SERVICE,
     &request,sizeof request,&result,sizeof result,&size);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(size!=sizeof result||result.accepted!=1||result.trained!=0||
    !terminated(result.status,sizeof result.status)||
    strcmp(result.status,"queued_for_review"))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,&result,sizeof result);
 *used=sizeof result;
 return STNLABZ_MODULE_OK;
}
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
/* Submit the exact observed IRC request and the exact produced Digit reply.
 * Match provenance against persisted Context. API acceptance means queued
 * for review; the API does not claim training completed. */
static int pair_valid(const digit_gru_learning_pair_request_t *q){
 return q&&terminated(q->channel,sizeof q->channel)&&
  !strcmp(q->channel,"#stn-labz")&&
  terminated(q->input,sizeof q->input)&&q->input[0]&&
  terminated(q->response,sizeof q->response)&&q->response[0]&&
  q->approved==1;
}
static stnlabz_module_result_t learning_pair(const void *raw,size_t bytes,
 void *response,size_t cap,size_t *used,void *context){
 const digit_gru_learning_pair_request_t *q=raw;
 digit_context_recent_query_t query={0};
 digit_context_recent_result_t recent={0};
 digit_gru_learn_request_t req={0};
 digit_gru_learn_result_t result={0};
 stnlabz_module_result_t rc;
 size_t read=0;
 unsigned i;
 const char *source=NULL;
 int n;
 (void)context;
 if(used)*used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(bytes!=sizeof *q||!pair_valid(q)||!response||
    cap<sizeof result||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(query.channel,q->channel,sizeof query.channel);
 rc=host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE,&query,sizeof query,
                          &recent,sizeof recent,&read);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(read!=sizeof recent||recent.count>DIGIT_CONTEXT_MAX)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<recent.count;i++){
  const digit_context_item_t *item=&recent.items[i];
  if(!terminated(item->source_id,sizeof item->source_id)||
     !terminated(item->context_type,sizeof item->context_type)||
     !terminated(item->value,sizeof item->value))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!strcmp(item->context_type,"utterance.observed")&&
     input_matches_observation(item->value,q->input)&&item->source_id[0])
   source=item->source_id;
 }
 if(!source)return STNLABZ_MODULE_ERR_NOT_FOUND;
 memcpy(req.input,q->input,sizeof q->input);
 memcpy(req.response,q->response,sizeof q->response);
 /* [AI:GPT-6 | 2026-10-21 UTC] */
 (void)n;
 if(!source[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
 snprintf(req.source,sizeof req.source,"digit");
 /* [End AI:GPT-6] */
 req.approved=1;
 read=0;
 rc=host->invoke_service(DIGIT_GRU_LEARN_SERVICE,&req,sizeof req,
                          &result,sizeof result,&read);
 if(rc!=STNLABZ_MODULE_OK)return rc;
 if(read!=sizeof result||result.accepted!=1||result.trained!=0||
    !terminated(result.status,sizeof result.status)||
    strcmp(result.status,"queued_for_review"))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,&result,sizeof result);
 *used=sizeof result;
 return STNLABZ_MODULE_OK;
}
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-21 UTC]
 * Automatic, restart-resumable engineering data synchronization.
 * At-least-once delivery: a crash between API acceptance and cursor
 * checkpoint can retry a record; training API should de-duplicate source IDs.
 * A GRU API 202 means queued for Friday review/rebuild, NOT trained. */
#define GRU_SYNC_CHECKPOINT "/opt/digit/data/gru_learning.cursor"
static pthread_t sync_worker;
static atomic_int sync_running;
static int sync_started;
static int64_t sync_cursor[3];
static int checkpoint_save(void){
 char temp[256];
 FILE *fp;
 int fd;
 if(snprintf(temp,sizeof temp,"%s.tmp",GRU_SYNC_CHECKPOINT)<=0)return 0;
 fp=fopen(temp,"w");
 if(!fp)return 0;
 if(fprintf(fp,"%lld %lld %lld\n",
    (long long)sync_cursor[0],(long long)sync_cursor[1],
    (long long)sync_cursor[2])<0){fclose(fp);unlink(temp);return 0;}
 if(fflush(fp)!=0){fclose(fp);unlink(temp);return 0;}
 fd=fileno(fp);
 if(fd<0||fsync(fd)!=0){fclose(fp);unlink(temp);return 0;}
 if(fclose(fp)!=0){unlink(temp);return 0;}
 if(rename(temp,GRU_SYNC_CHECKPOINT)!=0){unlink(temp);return 0;}
 return 1;
}
static int checkpoint_load(void){
 FILE *fp=fopen(GRU_SYNC_CHECKPOINT,"r");
 long long a=0,b=0,c=0;
 if(!fp)return errno==ENOENT;
 if(fscanf(fp,"%lld %lld %lld",&a,&b,&c)!=3||
    a<0||b<0||c<0){fclose(fp);return 0;}
 fclose(fp);
 sync_cursor[0]=a;sync_cursor[1]=b;sync_cursor[2]=c;
 return 1;
}
static int feed_one(unsigned index){
 digit_gru_feed_query_t query={0};
 digit_gru_feed_result_t page={0};
 digit_gru_learn_request_t learn={0};
 digit_gru_learn_result_t receipt={0};
 stnlabz_module_result_t rc;
 size_t used=0;
 const digit_gru_feed_item_t *row;
 const char *kind;
 int written;
 query.source_type=index+1;
 query.after_id=sync_cursor[index];
 rc=host->invoke_service(DIGIT_SQLITE3_GRU_FEED_SERVICE,
      &query,sizeof query,&page,sizeof page,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof page||
    page.count>DIGIT_GRU_FEED_PAGE_SIZE)return -1;
 if(!page.count)return 0;
 row=&page.items[0];
 if(row->id<=query.after_id||
    !terminated(row->text,sizeof row->text)||
    !terminated(row->category,sizeof row->category)||
    !terminated(row->verification_status,sizeof row->verification_status))
    return -1;
 kind=index==0?"Corpus":index==1?"Observation":"Context";
 /* The question is metadata, not a fabricated user question. The response
  * is a byte-for-byte copy of the SQLite feed's sanitized text. */
 written=snprintf(learn.input,sizeof learn.input,
     "Digit %s record %lld, category %.60s, verification %.44s:",
     kind,(long long)row->id,row->category,row->verification_status);
 if(written<=0||(size_t)written>=sizeof learn.input)return -1;
 if(!row->text[0]){
  sync_cursor[index]=row->id;
  return checkpoint_save()?1:-1;
 }
 memcpy(learn.response,row->text,sizeof row->text);
 snprintf(learn.source,sizeof learn.source,"digit");
 learn.approved=1;
 used=0;
 rc=host->invoke_service(DIGIT_GRU_LEARN_SERVICE,
      &learn,sizeof learn,&receipt,sizeof receipt,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof receipt||
    receipt.accepted!=1||receipt.trained!=0||
    !terminated(receipt.status,sizeof receipt.status)||
    strcmp(receipt.status,"queued_for_review"))return -1;
 sync_cursor[index]=row->id;
 return checkpoint_save()?1:-1;
}
static void *sync_loop(void *arg){
 unsigned source=0;
 struct timespec delay={1,0};
 (void)arg;
 while(atomic_load(&sync_running)){
  int result=feed_one(source);
  source=(source+1)%3;
  /* Back-pressure and retries; no IRC chatter, no uncontrolled bursts. */
  if(result<0)syslog(LOG_WARNING,
       "digit.gru_learning sync paused/retrying source=%u",source);
  nanosleep(&delay,NULL);
 }
 return NULL;
}
/* [End AI:GPT-6] */
static void check(stnlabz_module_qualification_result_t *r,int pass,int negative){
 r->tests_executed++;
 if(pass)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(pass)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 digit_gru_learning_preview_request_t request={0};
 size_t i;
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 check(r,!validate(&request),1);
 check(r,!validate(NULL),1);
 strcpy(request.channel,"#digit");
 check(r,validate(&request),0);
 request.channel[0]=0;
 strcpy(request.query,"PHP");
 check(r,validate(&request),0);
 strcpy(request.channel,"#digit");
 check(r,validate(&request),0);
 request.query[0]=0;
 check(r,validate(&request),0);
 request.channel[0]=0;
 check(r,!validate(&request),1);
 for(i=1;i<=6;i++){
  memset(request.channel,'a',sizeof request.channel);
  request.channel[i]=0;
  check(r,validate(&request),0);
 }
 memset(request.channel,'a',sizeof request.channel);
 check(r,!validate(&request),1);
 memset(&request,0,sizeof request);
 for(i=1;i<=6;i++){
  memset(request.query,'b',sizeof request.query);
  request.query[i]=0;
  check(r,validate(&request),0);
 }
 memset(request.query,'b',sizeof request.query);
 check(r,!validate(&request),1);
 memset(&request,0,sizeof request);
 request.query[511]=0;request.query[0]='a';
 check(r,validate(&request),0);
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
 {
 digit_gru_learning_submit_request_t s={0};
 check(r,!submit_valid(&s),1);
 strcpy(s.channel,"#stn-labz");
 strcpy(s.event_id,"recorded-event");
 strcpy(s.response,"Reviewed response");
 check(r,!submit_valid(&s),1);
 s.approved=1;check(r,submit_valid(&s),0);
 s.approved=2;check(r,!submit_valid(&s),1);
 s.approved=1;s.event_id[0]=0;check(r,!submit_valid(&s),1);
 strcpy(s.event_id,"recorded-event");
 s.response[0]=0;check(r,!submit_valid(&s),1);
 strcpy(s.response,"Reviewed response");
 strcpy(s.channel,"#other");check(r,!submit_valid(&s),1);
 strcpy(s.channel,"#stn-labz");check(r,submit_valid(&s),0);
 check(r,!submit_valid(NULL),1);
 }
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
 { digit_gru_learning_exchange_request_t e={0};
 check(r,!exchange_valid(&e),1);
 strcpy(e.channel,"#stn-labz");strcpy(e.input,"What is PHP?");
 strcpy(e.response,"Corpus #42 excerpt: PHP is a language.");
 e.corpus_id=42;
 check(r,!exchange_valid(&e),1);
 strcpy(e.verification_status,"verified");
 check(r,exchange_valid(&e),0);
 strcpy(e.verification_status,"pending");
 check(r,!exchange_valid(&e),1);
 strcpy(e.verification_status,"operator-verified");
 check(r,exchange_valid(&e),0);
 e.corpus_id=43;check(r,!exchange_valid(&e),1);
 e.corpus_id=42;strcpy(e.channel,"#other");
 check(r,!exchange_valid(&e),1);
 strcpy(e.channel,"#stn-labz");check(r,exchange_valid(&e),0);
 check(r,input_matches_observation("Digit: What is PHP?","What is PHP?"),0);
 check(r,!input_matches_observation("not a match","What is PHP?"),1);
 }
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
 {digit_gru_learning_pair_request_t p={0};
 check(r,!pair_valid(&p),1);
 strcpy(p.channel,"#stn-labz");
 strcpy(p.input,"digit tell me about PHP");
 strcpy(p.response,"PHP is a programming language.");
 check(r,!pair_valid(&p),1);
 p.approved=1;check(r,pair_valid(&p),0);
 p.approved=0;check(r,!pair_valid(&p),1);
 p.approved=1;strcpy(p.channel,"#other");
 check(r,!pair_valid(&p),1);
 strcpy(p.channel,"#stn-labz");
 check(r,pair_valid(&p),0);
 }
/* [End AI:GPT-6] */
 return r->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&
        !r->tests_failed&&r->negative_test_executed&&r->negative_test_passed?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->invoke_service||!api->register_service||
    !api->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!api->register_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,preview,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
 if(!api->register_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,submit,NULL)){
  api->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
 if(!api->register_service(DIGIT_GRU_LEARNING_EXCHANGE_SERVICE,exchange,NULL)){
  api->unregister_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
/* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
 if(!api->register_service(DIGIT_GRU_LEARNING_PAIR_SERVICE,learning_pair,NULL)){
  api->unregister_service(DIGIT_GRU_LEARNING_EXCHANGE_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 /* [End AI:GPT-6] */
 /* [AI:GPT-6 | 2026-10-21 UTC] */
 if(!checkpoint_load()){
  api->unregister_service(DIGIT_GRU_LEARNING_PAIR_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_EXCHANGE_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=api;
 atomic_store(&sync_running,1);
 if(pthread_create(&sync_worker,NULL,sync_loop,NULL)!=0){
  atomic_store(&sync_running,0);
  host=NULL;
  api->unregister_service(DIGIT_GRU_LEARNING_PAIR_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_EXCHANGE_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,NULL);
  api->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 sync_started=1;
 return STNLABZ_MODULE_OK;
 /* [End AI:GPT-6] */
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 /* [AI:GPT-6 | 2026-10-21 UTC] */
 atomic_store(&sync_running,0);
 if(sync_started){pthread_join(sync_worker,NULL);sync_started=0;}
 /* [End AI:GPT-6] */
/* [AI:GPT-6 | 2026-10-21 05:41:12 UTC] */
/* [AI:GPT-6 | 2026-10-20 UTC] */
 if(!host->unregister_service(DIGIT_GRU_LEARNING_PAIR_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_GRU_LEARNING_EXCHANGE_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
/* [End AI:GPT-6] */
 if(!host->unregister_service(DIGIT_GRU_LEARNING_SUBMIT_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
/* [End AI:GPT-6] */
 if(!host->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="gru_learning",.name="GRU Learning IRC Evidence Preview and Approved Submission",
 .version_major=0,.version_minor=7,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
