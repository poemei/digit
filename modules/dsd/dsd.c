/* Digit DSD: generic bounded trace diagnostics; no repair authority. */
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
static _Thread_local uint64_t current_trace;
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
 if(!strcmp(status,"TRACE_BOUND"))return "request trace was bound to the dispatch thread";
 if(!strcmp(status,"RESPONSE_RETURNED"))return "a response was returned by the service";
 if(!strcmp(status,"QUERY_MATCH"))return "a corpus match was returned";
 if(!strcmp(status,"QUERY_NO_MATCH"))return "the corpus search returned no matching records";
 if(!strcmp(status,"QUERY_ERROR"))return "the corpus search failed";
 if(!strcmp(status,"SERVICE_OK"))return "the service completed without reporting an error";
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
/* The entire retained event trail is returned in out->events. The bounded
 * summary describes the earliest recorded failure (or final observation),
 * without truncating its status meaning or inventing deeper execution. */
static int diagnose(uint64_t trace_id,digit_dsd_result_t *out){
 const trace_slot_t *slot;
 unsigned i,selected=0;
 const digit_dsd_event_t *e;
 const char *meaning;
 int n;
 if(!out||!trace_id)return 0;
 memset(out,0,sizeof *out);
 out->trace_id=trace_id;
 pthread_mutex_lock(&guard);
 slot=find(trace_id);
 if(slot){
  out->found=1;
  out->count=slot->count;
  out->dropped=slot->dropped;
  memcpy(out->events,slot->events,slot->count*sizeof slot->events[0]);
 }
 pthread_mutex_unlock(&guard);
 if(!out->found){
  snprintf(out->summary,sizeof out->summary,
           "Trace not retained or not observed.");
  return 1;
 }
 for(i=0;i<out->count;i++){
  if(out->events[i].result){
   if(!out->failures)selected=i;
   out->failures++;
  }
 }
 if(!out->count){
  snprintf(out->summary,sizeof out->summary,
           "Trace contains no recorded operations.");
  return 1;
 }
 if(!out->failures)selected=out->count-1;
 e=&out->events[selected];
 meaning=status_meaning(e->status);
 /* Report a bounded timeline, not just the final IRC observation. */
 {
  size_t used=0;unsigned j;
  meaning=status_meaning(e->status);
  n=snprintf(out->summary,sizeof out->summary,
    "%u observed operation(s), %u failed. %s: %.48s. ",
    out->count,out->failures,
    out->failures?"First failure":"Last observation",
    meaning?meaning:e->status);
  if(n<0||(size_t)n>=sizeof out->summary)return 0;
  used=(size_t)n;
  for(j=0;j<out->count;j++){
   const digit_dsd_event_t *step=&out->events[j];
   n=snprintf(out->summary+used,sizeof out->summary-used,
     "%s%.20s.%.22s -> %.28s [%.25s, records=%u] at %.37s:%u;",
     j?" ":"",step->module,step->method,step->operation,
     step->status,step->records,step->file,step->line);
   if(n<0)return 0;
   if((size_t)n>=sizeof out->summary-used)break;
   used+=(size_t)n;
  }
  if(j<out->count&&used+36<sizeof out->summary){
   n=snprintf(out->summary+used,sizeof out->summary-used,
      " %u more retained event(s);",out->count-j);
   if(n>0&&(size_t)n<sizeof out->summary-used)used+=(size_t)n;
  }
  meaning=status_meaning(e->status);
  if(used+92<sizeof out->summary){
   n=snprintf(out->summary+used,sizeof out->summary-used,
     " %s",
     out->failures?"Underlying cause unverified.":"End-to-end completion unverified.");
   if(n>0&&(size_t)n<sizeof out->summary-used)used+=(size_t)n;
  }
  if(out->dropped&&used+32<sizeof out->summary)
   (void)snprintf(out->summary+used,sizeof out->summary-used,
     " %u event(s) dropped.",out->dropped);
 }

 return 1;
}
static stnlabz_module_result_t bind_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *ctx){
 (void)response;(void)response_size;(void)ctx;
 if(used)*used=0;
 if(!request||request_size!=sizeof(uint64_t))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(&current_trace,request,sizeof current_trace);
 if(current_trace){
  digit_dsd_event_t e={0};
  e.trace_id=current_trace;
  e.span_id=1;
  e.line=__LINE__;
  snprintf(e.module,sizeof e.module,"dsd");
  snprintf(e.method,sizeof e.method,"bind");
  snprintf(e.file,sizeof e.file,"modules/dsd/dsd.c");
  snprintf(e.status,sizeof e.status,"TRACE_BOUND");
  snprintf(e.operation,sizeof e.operation,"request_trace");
  if(!record(&e)){
   current_trace=0;
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  }
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t current_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *ctx){
 (void)ctx;
 if(used)*used=0;
 if((request_size!=sizeof(uint64_t)&&request_size!=0)||
    (request_size==sizeof(uint64_t)&&!request)||
    !response||response_size<sizeof current_trace||!used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(response,&current_trace,sizeof current_trace);
 *used=sizeof current_trace;
 return STNLABZ_MODULE_OK;
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
/* The caller authenticates the account and selects its previous trace.
 * Command words are developer-query controls, never request-entry verbs. */
static int command_text(const digit_dsd_command_request_t *req,
                        digit_dsd_command_result_t *out){
 digit_dsd_result_t diagnosis;
 uint64_t id=0;
 unsigned long long parsed=0;
 char tail;
 const char *cmd;
 int n;
 if(!req||!out||!memchr(req->command,0,sizeof req->command))return 0;
 memset(out,0,sizeof *out);
 cmd=req->command;
 if(!strcmp(cmd,"show dsd")){
  snprintf(out->text,sizeof out->text,
   "DSD available: bounded observed trace events. Commands: show trace | "
   "diagnose my last request. Unobserved execution cannot be diagnosed.");
  return 1;
 }
 if(!strcmp(cmd,"show trace")||!strcmp(cmd,"diagnose my last request")){
  id=req->previous_trace_id;
  if(!id){
   snprintf(out->text,sizeof out->text,
     "DSD: Previous private request trace unavailable.");
   return 1;
  }
 }else if(!strncmp(cmd,"diagnose trace ",15)){
  if(sscanf(cmd+15,"%llu%c",&parsed,&tail)!=1||!parsed){
   snprintf(out->text,sizeof out->text,"DSD: Invalid trace identifier.");
   return 1;
  }
  id=(uint64_t)parsed;
  if((unsigned long long)id!=parsed)return 0;
 }else if(!strncmp(cmd,"diagnose module ",16)){
  const char *module=cmd+16;
  if(!valid_code(module,strlen(module)+1,0)){
   snprintf(out->text,sizeof out->text,"DSD: Invalid module identifier.");
   return 1;
  }
  if(!latest_module_trace(module,&id)){
   snprintf(out->text,sizeof out->text,
    "DSD: No recorded observations for module %.48s. This does not establish module health.",
    module);
   return 1;
  }
 }else return 0;
 if(!diagnose(id,&diagnosis))return 0;
 if(!diagnosis.found){
  snprintf(out->text,sizeof out->text,
    "DSD: Previous request trace is no longer retained.");
  return 1;
 }
 if(!strcmp(cmd,"show trace")){
  /* A full list of events remains in digit.dsd.diagnose; this reply
   * is a bounded summary of their observed timeline. */
  n=snprintf(out->text,sizeof out->text,
   "DSD trace %llu: %.420s",
   (unsigned long long)id,diagnosis.summary);
 }else if(diagnosis.dropped){
  n=snprintf(out->text,sizeof out->text,
   "DSD trace %llu: %.355s %u event(s) omitted; evidence incomplete.",
   (unsigned long long)id,diagnosis.summary,diagnosis.dropped);
 }else{
  n=snprintf(out->text,sizeof out->text,
   "DSD trace %llu: %.420s",
   (unsigned long long)id,diagnosis.summary);
 }
 return n>=0&&(size_t)n<sizeof out->text;
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

/* Qualifications exercise real code paths with synthetic sanitized events.
 * They never operate on the live database or assume an input verb. */
static void assertion(stnlabz_module_qualification_result_t *q,
                      int ok,int negative,const char *description){
 q->tests_executed++;
 if(ok)q->tests_passed++;
 else{
  q->tests_failed++;
  fprintf(stderr,"DSD qualification %u FAILED: %s\n",
          q->tests_executed,description);
 }
 if(negative){
  q->negative_test_executed=1;
  if(ok)q->negative_test_passed=1;
 }
}
#define CHECK(condition,negative) assertion(q,!!(condition),negative,#condition)
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_dsd_command_request_t cmd={0};
 digit_dsd_command_result_t reply;
 digit_dsd_event_t event={0},bad;
 digit_dsd_result_t result;
 unsigned i;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 event.trace_id=42;
 event.span_id=1;
 event.line=__LINE__;
 snprintf(event.module,sizeof event.module,"testmodule");
 snprintf(event.method,sizeof event.method,"read");
 snprintf(event.file,sizeof event.file,"test/diagnostics.c");
 snprintf(event.status,sizeof event.status,"SQLITE_BUSY");
 snprintf(event.operation,sizeof event.operation,"db.read");
 CHECK(valid_event(&event),0);
 CHECK(record(&event),0);
 CHECK(diagnose(42,&result)&&result.found&&result.count==1&&!result.failures,0);
 CHECK(strstr(result.summary,"database is locked")!=NULL,0);
 CHECK(strstr(result.summary,"End-to-end completion unverified")!=NULL,0);
 event.result=1;
 event.span_id=2;
 CHECK(record(&event),0);
 CHECK(diagnose(42,&result)&&result.count==2&&result.failures==1,0);
 CHECK(strstr(result.summary,"database is locked")!=NULL,0);
 CHECK(strstr(result.summary,"Underlying cause unverified")!=NULL,0);
 bad=event;bad.trace_id=0;CHECK(!valid_event(&bad),1);
 bad=event;bad.span_id=0;CHECK(!valid_event(&bad),1);
 bad=event;bad.result=3;CHECK(!valid_event(&bad),1);
 bad=event;bad.line=0;CHECK(!valid_event(&bad),1);
 bad=event;snprintf(bad.method,sizeof bad.method,"bad method");
 CHECK(!valid_event(&bad),1);
 bad=event;snprintf(bad.operation,sizeof bad.operation,"private=key");
 CHECK(!valid_event(&bad),1);
 bad=event;memset(bad.status,'x',sizeof bad.status);
 CHECK(!valid_event(&bad),1);
 CHECK(!diagnose(0,&result),1);
 CHECK(diagnose(999,&result)&&!result.found,0);
 {
  digit_dsd_event_t observed={0};
  observed.trace_id=55;
  observed.span_id=1;
  observed.line=__LINE__;
  snprintf(observed.module,sizeof observed.module,"adapter");
  snprintf(observed.method,sizeof observed.method,"dispatch");
  snprintf(observed.file,sizeof observed.file,"test/adapter.c");
  snprintf(observed.operation,sizeof observed.operation,"service.call");
  snprintf(observed.status,sizeof observed.status,"NO_RESPONSE");
  /* No reply is not a failed ABI service invocation. */
  CHECK(record(&observed),0);
  CHECK(diagnose(55,&result)&&result.count==1&&!result.failures,0);
  CHECK(strstr(result.summary,"request returned without a reply")!=NULL,0);
  CHECK(strstr(result.summary,"End-to-end completion unverified")!=NULL,0);
  observed.trace_id=56;observed.span_id=2;observed.result=1;
  snprintf(observed.status,sizeof observed.status,"SERVICE_NOT_FOUND");
  CHECK(record(&observed),0);
  CHECK(diagnose(56,&result)&&result.failures==1,0);
  CHECK(strstr(result.summary,"was not registered")!=NULL,0);
  observed.trace_id=57;observed.span_id=3;
  snprintf(observed.status,sizeof observed.status,"NOT_AUTHORIZED");
  CHECK(record(&observed),0);
  CHECK(diagnose(57,&result)&&result.failures==1,0);
  CHECK(strstr(result.summary,"not authorized")!=NULL,0);
  observed.trace_id=58;observed.span_id=4;
  snprintf(observed.status,sizeof observed.status,"INVALID_ARGUMENT");
  CHECK(record(&observed),0);
  CHECK(diagnose(58,&result)&&result.failures==1,0);
  CHECK(strstr(result.summary,"rejected its input")!=NULL,0);
 }
 /* A single trace must retain real intermediate stages and outcomes. */
 {
  digit_dsd_event_t stage={0};
  stage.trace_id=60;stage.span_id=1;stage.line=__LINE__;
  snprintf(stage.module,sizeof stage.module,"sqlite3");
  snprintf(stage.method,sizeof stage.method,"corpus_search");
  snprintf(stage.file,sizeof stage.file,"modules/sqlite3/sqlite3.c");
  snprintf(stage.status,sizeof stage.status,"QUERY_MATCH");
  snprintf(stage.operation,sizeof stage.operation,"corpus_fts");
  stage.records=3;
  CHECK(record(&stage),0);
  stage.span_id=2;stage.records=3;
  snprintf(stage.module,sizeof stage.module,"conversation");
  snprintf(stage.method,sizeof stage.method,"answer");
  snprintf(stage.file,sizeof stage.file,"modules/conversation/conversation.c");
  snprintf(stage.operation,sizeof stage.operation,"corpus_search");
  CHECK(record(&stage),0);
  stage.span_id=3;stage.records=0;
  snprintf(stage.module,sizeof stage.module,"irc");
  snprintf(stage.method,sizeof stage.method,"incoming");
  snprintf(stage.file,sizeof stage.file,"modules/irc/irc.c");
  snprintf(stage.operation,sizeof stage.operation,"digit.message");
  snprintf(stage.status,sizeof stage.status,"RESPONSE_RETURNED");
  CHECK(record(&stage),0);
  CHECK(diagnose(60,&result)&&result.count==3&&!result.failures,0);
  CHECK(strstr(result.summary,"sqlite3.corpus_search")!=NULL,0);
  CHECK(strstr(result.summary,"conversation.answer")!=NULL,0);
  CHECK(strstr(result.summary,"RESPONSE_RETURNED")!=NULL,0);
  cmd.previous_trace_id=60;
  snprintf(cmd.command,sizeof cmd.command,"show trace");
  CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"corpus_fts")!=NULL,0);
  cmd.previous_trace_id=0;
 }
 for(i=0;i<DIGIT_DSD_MAX_EVENTS+2;i++){
  event.span_id=3+i;
  CHECK(record(&event),0);
 }
 CHECK(diagnose(42,&result)&&result.count==DIGIT_DSD_MAX_EVENTS&&result.dropped==4,0);
 CHECK(result.events[0].span_id==1&&result.events[1].span_id==2,0);
 snprintf(cmd.command,sizeof cmd.command,"show dsd");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"DSD available")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"show trace");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"unavailable")!=NULL,0);
 cmd.previous_trace_id=42;
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"DSD trace 42")!=NULL,0);
 cmd.previous_trace_id=0;
 snprintf(cmd.command,sizeof cmd.command,"diagnose my last request");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"unavailable")!=NULL,0);
 cmd.previous_trace_id=42;
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"database is locked")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose trace 42");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"database is locked")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose trace nonsense");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"Invalid trace")!=NULL,1);
 snprintf(cmd.command,sizeof cmd.command,"diagnose module nonexistent");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"No recorded observations")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"diagnose module testmodule");
 CHECK(command_text(&cmd,&reply)&&strstr(reply.text,"database is locked")!=NULL,0);
 snprintf(cmd.command,sizeof cmd.command,"hello");
 CHECK(!command_text(&cmd,&reply),1);
 pthread_mutex_lock(&guard);
 memset(slots,0,sizeof slots);
 replacement=0;
 chronology=0;
 pthread_mutex_unlock(&guard);
 return q->tests_executed>=20&&!q->tests_failed&&
        q->tests_executed==q->tests_passed&&
        q->negative_test_executed&&q->negative_test_passed?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
#undef CHECK
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service(DIGIT_DSD_RECORD_SERVICE,record_service,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(DIGIT_DSD_DIAGNOSE_SERVICE,diagnose_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!h->register_service(DIGIT_DSD_BIND_SERVICE,bind_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_DIAGNOSE_SERVICE,NULL);
  (void)h->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!h->register_service(DIGIT_DSD_CURRENT_SERVICE,current_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_BIND_SERVICE,NULL);
  (void)h->unregister_service(DIGIT_DSD_DIAGNOSE_SERVICE,NULL);
  (void)h->unregister_service(DIGIT_DSD_RECORD_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!h->register_service(DIGIT_DSD_COMMAND_SERVICE,command_service,NULL)){
  (void)h->unregister_service(DIGIT_DSD_CURRENT_SERVICE,NULL);
  (void)h->unregister_service(DIGIT_DSD_BIND_SERVICE,NULL);
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
 if(!host->unregister_service(DIGIT_DSD_CURRENT_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_DSD_BIND_SERVICE,NULL))
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
 .version_major=0,.version_minor=7,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
