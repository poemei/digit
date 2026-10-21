/* Read-only learning-material preview, ABI 1.5.
 * Observations are claims about speech; Corpus excerpts retain their
 * verification metadata. No examples are approved, queued, or trained.
 * This module does not fabricate response pairs or read SQLite directly. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
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
 return r->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&
        !r->tests_failed&&r->negative_test_executed&&r->negative_test_passed?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->invoke_service||!api->register_service||
    !api->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!api->register_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,preview,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=api;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_GRU_LEARNING_PREVIEW_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="gru_learning",.name="GRU Learning Material Preview",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
