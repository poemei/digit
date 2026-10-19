/* Digit DSD 0.1.0: bounded trace diagnostics; no repair authority. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "dsd.h"
#define SLOTS 64
typedef struct {
 uint64_t id;
 unsigned int count,dropped;
 digit_dsd_event_t events[DIGIT_DSD_MAX_EVENTS];
} trace_slot_t;
static trace_slot_t slots[SLOTS];
static unsigned int replacement;
static pthread_mutex_t guard=PTHREAD_MUTEX_INITIALIZER;
static const stnlabz_module_host_t *host;
static int valid_code(const char *p,size_t cap,int path){
 size_t i;
 if(!p||!memchr(p,0,cap)||!p[0])return 0;
 for(i=0;p[i];i++){
  unsigned char c=(unsigned char)p[i];
  if(!(isalnum(c)||c=='_'||c=='-'||c=='.'||
      (path&&(c=='/'||c==':'))))return 0;
 }
 return 1;
}
static int valid_event(const digit_dsd_event_t *e){
 return e && e->trace_id && e->span_id && e->result<=2 &&
  valid_code(e->module,sizeof e->module,0) &&
  valid_code(e->method,sizeof e->method,0) &&
  valid_code(e->file,sizeof e->file,1) &&
  valid_code(e->status,sizeof e->status,0) &&
  valid_code(e->operation,sizeof e->operation,0) &&
  e->line>0;
}
static trace_slot_t *find(uint64_t id){
 unsigned i;
 for(i=0;i<SLOTS;i++)if(slots[i].id==id)return &slots[i];
 return NULL;
}
static int record(const digit_dsd_event_t *event){
 trace_slot_t *slot;
 unsigned i;
 if(!valid_event(event))return 0;
 pthread_mutex_lock(&guard);
 slot=find(event->trace_id);
 if(!slot){
  for(i=0;i<SLOTS;i++)if(!slots[i].id){slot=&slots[i];break;}
  if(!slot){slot=&slots[replacement++%SLOTS];}
  memset(slot,0,sizeof *slot);slot->id=event->trace_id;
 }
 if(slot->count<DIGIT_DSD_MAX_EVENTS)slot->events[slot->count++]=*event;
 else slot->dropped++;
 pthread_mutex_unlock(&guard);
 return 1;
}
static int diagnose(uint64_t trace_id,digit_dsd_result_t *out){
 trace_slot_t *slot;
 unsigned i,first_failure=DIGIT_DSD_MAX_EVENTS;
 if(!out||!trace_id)return 0;
 memset(out,0,sizeof *out);out->trace_id=trace_id;
 pthread_mutex_lock(&guard);
 slot=find(trace_id);
 if(slot){
  out->found=1;out->count=slot->count;out->dropped=slot->dropped;
  memcpy(out->events,slot->events,slot->count*sizeof slot->events[0]);
 }
 pthread_mutex_unlock(&guard);
 if(!out->found){
  snprintf(out->summary,sizeof out->summary,"Trace not retained or not observed.");
  return 1;
 }
 for(i=0;i<out->count;i++)if(out->events[i].result){
  out->failures++;
  if(first_failure==DIGIT_DSD_MAX_EVENTS)first_failure=i;
 }
 if(first_failure!=DIGIT_DSD_MAX_EVENTS){
  const digit_dsd_event_t *e=&out->events[first_failure];
  snprintf(out->summary,sizeof out->summary,
   "Observed failure boundary: %.47s.%.63s at %.159s:%u, operation %.63s, status %.63s. Underlying cause not established.",
   e->module,e->method,e->file,e->line,e->operation,e->status);
 }else{
  snprintf(out->summary,sizeof out->summary,
   "No failure recorded in %u observed event(s). This is not proof of end-to-end success.",
   out->count);
 }
 return 1;
}
static stnlabz_module_result_t record_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *ctx){
 const digit_dsd_record_t *r=request;
 (void)response;(void)response_size;(void)ctx;
 if(used)*used=0;
 if(!r||request_size!=sizeof *r||r->trace_id!=r->event.trace_id ||
    !record(&r->event))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t diagnose_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *ctx){
 const digit_dsd_query_t *q=request;
 (void)ctx;
 if(used)*used=0;
 if(!q||request_size!=sizeof *q||!response||response_size<sizeof(digit_dsd_result_t)||!used||
    !diagnose(q->trace_id,(digit_dsd_result_t *)response))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *used=sizeof(digit_dsd_result_t);
 return STNLABZ_MODULE_OK;
}
static void assertion(stnlabz_module_qualification_result_t *q,int ok,int negative){
 q->tests_executed++;if(ok)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(ok)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_dsd_event_t e={0},bad;
 digit_dsd_result_t out;
 unsigned i;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 e.trace_id=42;e.span_id=1;e.line=17;
 snprintf(e.module,sizeof e.module,"sqlite3");
 snprintf(e.method,sizeof e.method,"get");
 snprintf(e.file,sizeof e.file,"modules/sqlite3/sqlite3.c");
 snprintf(e.status,sizeof e.status,"SQLITE_BUSY");
 snprintf(e.operation,sizeof e.operation,"select");
 assertion(q,valid_event(&e),0);
 assertion(q,record(&e),0);
 assertion(q,diagnose(42,&out)&&out.found&&out.count==1&&!out.failures,0);
 e.result=1;e.span_id=2;
 assertion(q,record(&e),0);
 assertion(q,diagnose(42,&out)&&out.failures==1&&
    strstr(out.summary,"SQLITE_BUSY")!=NULL,0);
 bad=e;bad.trace_id=0;assertion(q,!valid_event(&bad),1);
 bad=e;bad.span_id=0;assertion(q,!valid_event(&bad),1);
 bad=e;bad.result=3;assertion(q,!valid_event(&bad),1);
 bad=e;bad.line=0;assertion(q,!valid_event(&bad),1);
 bad=e;snprintf(bad.method,sizeof bad.method,"a b");
 assertion(q,!valid_event(&bad),1);
 bad=e;snprintf(bad.operation,sizeof bad.operation,"key=private");
 assertion(q,!valid_event(&bad),1);
 bad=e;snprintf(bad.file,sizeof bad.file,"../../x");
 assertion(q,valid_event(&bad),0); /* location is never treated as verified */
 bad=e;memset(bad.status,'x',sizeof bad.status);
 assertion(q,!valid_event(&bad),1);
 assertion(q,!diagnose(0,&out),1);
 assertion(q,diagnose(999,&out)&&!out.found,0);
 for(i=0;i<DIGIT_DSD_MAX_EVENTS+2;i++){
  e.span_id=3+i;assertion(q,record(&e),0);
 }
 assertion(q,diagnose(42,&out)&&out.count==DIGIT_DSD_MAX_EVENTS&&out.dropped==4,0);
 assertion(q,out.events[0].span_id==1&&out.events[1].span_id==2,0);
 /* Qualification has no external side effects and resets its own fixture. */
 pthread_mutex_lock(&guard);memset(slots,0,sizeof slots);replacement=0;
 pthread_mutex_unlock(&guard);
 return q->tests_executed>=20&&!q->tests_failed?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service(DIGIT_DSD_RECORD_SERVICE,record_service,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(DIGIT_DSD_DIAGNOSE_SERVICE,diagnose_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_DSD_DIAGNOSE_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;
 pthread_mutex_lock(&guard);memset(slots,0,sizeof slots);replacement=0;
 pthread_mutex_unlock(&guard);
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="dsd",.name="Developer Self-Diagnostics",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
