/* Digit DSD 0.1.0: bounded trace diagnostics; no repair authority. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "dsd.h"
#define SLOTS 64
typedef struct {
 uint64_t id,updated;
 unsigned int count,dropped;
 digit_dsd_event_t events[DIGIT_DSD_MAX_EVENTS];
} trace_slot_t;
static trace_slot_t slots[SLOTS];
static unsigned int replacement;
static uint64_t chronology;
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
 slot->updated=++chronology;
 if(slot->count<DIGIT_DSD_MAX_EVENTS)slot->events[slot->count++]=*event;
 else slot->dropped++;
 pthread_mutex_unlock(&guard);
 return 1;
}
/* Human-readable statuses are presentation only; original evidence stays intact. */
static const char *status_meaning(const char *status){
 if(!strcmp(status,"NO_RESPONSE"))return "request returned without a reply";
 if(!strcmp(status,"SERVICE_NOT_FOUND"))return "the requested service was not registered";
 if(!strcmp(status,"NOT_AUTHORIZED"))return "the request was not authorized";
 if(!strcmp(status,"INVALID_ARGUMENT"))return "the service rejected its input";
 if(!strcmp(status,"INVALID_STATE"))return "the service was not ready for this operation";
 if(!strcmp(status,"QUARANTINED"))return "the target module was quarantined";
 if(!strcmp(status,"NOT_QUALIFIED"))return "the target module was not qualified";
 if(!strcmp(status,"SERVICE_ERROR"))return "the service returned an error";
 if(!strcmp(status,"SQLITE_BUSY"))return "database is locked";
 if(!strcmp(status,"SQLITE_LOCKED"))return "database table is locked";
 if(!strcmp(status,"SQLITE_CORRUPT"))return "database appears malformed";
 if(!strcmp(status,"SQLITE_READONLY"))return "database is read-only";
 if(!strcmp(status,"SQLITE_IOERR"))return "database I/O failed";
 if(!strcmp(status,"SERVICE_TIMEOUT"))return "service did not respond in time";
 if(!strcmp(status,"SERVICE_UNAVAILABLE"))return "service is unavailable";
 if(!strcmp(status,"MODULE_NOT_FOUND"))return "module was not found";
 return NULL;
}
static int latest_module_trace(const char *module,uint64_t *id){
 unsigned i,j;uint64_t newest=0;
 int found=0;
 if(!module||!id)return 0;
 pthread_mutex_lock(&guard);
 for(i=0;i<SLOTS;i++){
  const trace_slot_t *slot=&slots[i];
  if(!slot->id||slot->updated<=newest)continue;
  for(j=0;j<slot->count;j++)
   if(!strcmp(slot->events[j].module,module)){
    newest=slot->updated;*id=slot->id;found=1;break;
   }
 }
 pthread_mutex_unlock(&guard);
 return found;
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
 /* Summarize all observed stages, not a hardcoded request or service.
  * The record's result field is the actual operation disposition. A
  * successful invocation without an answer is not a fabricated failure. */
 {
  size_t used=0;
  int n;
  const char *assessment=first_failure==DIGIT_DSD_MAX_EVENTS?
     "No recorded operation failed; end-to-end completion unverified.":
     "A recorded operation failed; underlying cause unverified.";
  n=snprintf(out->summary,sizeof out->summary,
    "Observed %u operation(s), %u failure(s). ",out->count,out->failures);
  if(n<0||(size_t)n>=sizeof out->summary)return 0;
  used=(size_t)n;
  for(i=0;i<out->count&&used+78<sizeof out->summary;i++){
   const digit_dsd_event_t *e=&out->events[i];
   const char *meaning=status_meaning(e->status);
   n=snprintf(out->summary+used,sizeof out->summary-used,
     "%s%.32s.%.42s -> %.42s [%s%.38s] at %.80s:%u.",
     i?" ":"",e->module,e->method,e->operation,
     e->result?"FAILED: ":"",meaning?meaning:e->status,e->file,e->line);
   if(n<0)return 0;
   if((size_t)n>=sizeof out->summary-used){
    used=sizeof out->summary-1;
    break;
   }
   used+=(size_t)n;
  }
  if(i<out->count&&used+40<sizeof out->summary){
   n=snprintf(out->summary+used,sizeof out->summary-used,
      " %u additional operation(s) retained.",out->count-i);
   if(n>0&&(size_t)n<sizeof out->summary-used)used+=(size_t)n;
  }
  if(used+strlen(assessment)+2<sizeof out->summary)
   snprintf(out->summary+used,sizeof out->summary-used," %s",assessment);
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

/* Read-only command language. No implicit global "last request" shared
 * across IRC accounts; the authorized entry adapter supplies that ID. */
static int command_text(const digit_dsd_command_request_t *req,
                        digit_dsd_command_result_t *out){
 digit_dsd_result_t diagnosis;
 uint64_t id=0;
 char tail;
 const char *text;
 if(!req||!out||!memchr(req->command,0,sizeof req->command))return 0;
 text=req->command;
 memset(out,0,sizeof *out);
 if(!strcmp(text,"show dsd")){
  snprintf(out->text,sizeof out->text,
    "DSD available: trace diagnostics, bounded evidence, source locations when instrumented. Commands: show trace | diagnose my last request. Uninstrumented paths remain unknown.");
  return 1;
 }
 if(!strcmp(text,"show trace")){
  id=req->previous_trace_id;
  if(!id){
   snprintf(out->text,sizeof out->text,"DSD: No previous private request trace is retained.");
   return 1;
  }
  if(!diagnose(id,&diagnosis))return 0;
  if(!diagnosis.found||!diagnosis.count){
   snprintf(out->text,sizeof out->text,"DSD: Previous request trace is no longer retained.");
   return 1;
  }
  {
   const digit_dsd_event_t *e=&diagnosis.events[0];
   snprintf(out->text,sizeof out->text,
     "DSD trace %llu: %u observed operation(s); first: %.40s.%.55s -> %.55s, status %.55s. Last instrumented source %.110s:%u. Downstream execution not recorded unless explicitly observed.",
     (unsigned long long)id,diagnosis.count,e->module,e->method,
     e->operation,e->status,e->file,e->line);
  }
  return 1;
 }
 if(!strcmp(text,"diagnose my last request")){
  id=req->previous_trace_id;
  if(!id){
   snprintf(out->text,sizeof out->text,
    "DSD: Previous request trace unavailable. No diagnosis can be established.");
   return 1;
  }
 }else if(!strncmp(text,"diagnose trace ",15)){
  unsigned long long parsed=0;
  if(sscanf(text+15,"%llu%c",&parsed,&tail)!=1||!parsed){
   snprintf(out->text,sizeof out->text,"DSD: Invalid trace identifier.");
   return 1;
  }
  id=(uint64_t)parsed;
  if((unsigned long long)id!=parsed)return 0;
 }else if(!strncmp(text,"diagnose module ",16)){
  const char *module=text+16;
  if(!valid_code(module,strlen(module)+1,0)){
   snprintf(out->text,sizeof out->text,"DSD: Invalid module identifier.");
   return 1;
  }
  if(!latest_module_trace(module,&id)){
   snprintf(out->text,sizeof out->text,
    "DSD: No recorded observations for module %.48s. This does not establish module health.",module);
   return 1;
  }
 }else return 0; /* Do not commandeer ordinary conversation. */
 if(!diagnose(id,&diagnosis))return 0;
 if(diagnosis.dropped){
  snprintf(out->text,sizeof out->text,
   "DSD trace %llu: %.340s %u event(s) omitted; evidence incomplete.",
   (unsigned long long)id,diagnosis.summary,diagnosis.dropped);
 }else{
  snprintf(out->text,sizeof out->text,"DSD trace %llu: %.420s",
   (unsigned long long)id,diagnosis.summary);
 }
 return 1;
}
static stnlabz_module_result_t command_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *context){
 (void)context;
 if(used)*used=0;
 if(!request||request_size!=sizeof(digit_dsd_command_request_t)||
    !response||response_size<sizeof(digit_dsd_command_result_t)||!used||
    !command_text((const digit_dsd_command_request_t *)request,
     (digit_dsd_command_result_t *)response))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *used=sizeof(digit_dsd_command_result_t);
 return STNLABZ_MODULE_OK;
}

static void assertion(stnlabz_module_qualification_result_t *q,int ok,int negative){
 q->tests_executed++;if(ok)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(ok)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_dsd_command_request_t cmd={0};digit_dsd_command_result_t reply;
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
 /* Silent digit.message is a service-boundary observation, not proof
  * of a faulty IRC transport or a particular Core line. */
 {
  digit_dsd_event_t silent={0};
  silent.trace_id=55;silent.span_id=1;silent.line=347;silent.result=1;
  snprintf(silent.module,sizeof silent.module,"irc");
  snprintf(silent.method,sizeof silent.method,"incoming");
  snprintf(silent.file,sizeof silent.file,"modules/irc/irc.c");
  snprintf(silent.operation,sizeof silent.operation,"digit.message");
  snprintf(silent.status,sizeof silent.status,"NO_RESPONSE");
  assertion(q,record(&silent),0);
  assertion(q,diagnose(55,&out) &&
    strstr(out.summary,"request returned without a reply")!=NULL &&
    strstr(out.summary,"underlying cause unverified")!=NULL,0);
  snprintf(silent.status,sizeof silent.status,"SERVICE_NOT_FOUND");
  silent.trace_id=56;silent.span_id=2;
  assertion(q,record(&silent),0);
  assertion(q,diagnose(56,&out)&&
    strstr(out.summary,"was not registered")!=NULL,0);
  snprintf(silent.status,sizeof silent.status,"NOT_AUTHORIZED");
  silent.trace_id=57;silent.span_id=3;
  assertion(q,record(&silent),0);
  assertion(q,diagnose(57,&out)&&
    strstr(out.summary,"not authorized")!=NULL,0);
  snprintf(silent.status,sizeof silent.status,"INVALID_ARGUMENT");
  silent.trace_id=58;silent.span_id=4;
  assertion(q,record(&silent),0);
  assertion(q,diagnose(58,&out)&&
    strstr(out.summary,"rejected its input")!=NULL,0);
 }
 for(i=0;i<DIGIT_DSD_MAX_EVENTS+2;i++){
  e.span_id=3+i;assertion(q,record(&e),0);
 }
 assertion(q,diagnose(42,&out)&&out.count==DIGIT_DSD_MAX_EVENTS&&out.dropped==4,0);
 assertion(q,out.events[0].span_id==1&&out.events[1].span_id==2,0);
 snprintf(cmd.command,sizeof cmd.command,"show dsd");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"DSD available")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"show trace");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"No previous private request")!=NULL,0);
 cmd.previous_trace_id=42;
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"observed operation")!=NULL,0);
 cmd.previous_trace_id=0;
 snprintf(cmd.command,sizeof cmd.command,"diagnose my last request");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"unavailable")!=NULL,0);
 cmd.previous_trace_id=42;
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"SQLITE_BUSY")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose trace 42");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"SQLITE_BUSY")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose trace nonsense");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"Invalid trace")!=NULL,1);
 snprintf(cmd.command,sizeof cmd.command,"diagnose module s2");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"No recorded observations")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose module sqlite3");
 assertion(q,command_text(&cmd,&reply)&&strstr(reply.text,"SQLITE_BUSY")!=NULL,0);
 assertion(q,strstr(reply.text,"database is locked")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"hello");
 assertion(q,!command_text(&cmd,&reply),1);
 /* Qualification has no external side effects and resets its own fixture. */
 pthread_mutex_lock(&guard);memset(slots,0,sizeof slots);replacement=0;chronology=0;
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
 if(!h->register_service(DIGIT_DSD_COMMAND_SERVICE,command_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_DIAGNOSE_SERVICE,NULL);
  (void)h->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_DSD_COMMAND_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
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
 .version_major=0,.version_minor=5,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
