/* Digit DSD: generic bounded trace diagnostics; no repair authority. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "stnlabz_abi/dsd.h"
#include "core_dsd.h"
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
 if(!strcmp(status,"TRACE_BIND_FAILED"))return "request trace binding or verification failed";
 if(!strcmp(status,"TRACE_BOUND"))return "request trace was bound to the dispatch thread";
 if(!strcmp(status,"RESPONSE_RETURNED"))return "a response was returned by the service";
 if(!strcmp(status,"QUERY_MATCH"))return "a corpus match was returned";
 if(!strcmp(status,"QUERY_NO_MATCH"))return "the corpus search returned no matching records";
 if(!strcmp(status,"QUERY_ERROR"))return "the corpus search failed";
 if(!strcmp(status,"SERVICE_OK"))return "the service completed without reporting an error";
 if(!strcmp(status,"NO_RESPONSE"))return "request returned without a reply";
 if(!strcmp(status,"REASONING_ABSTAIN"))return "Reasoning abstained without claiming a failed retrieval";
 if(!strcmp(status,"NO_GENERATOR"))return "Conversation has no general response generator";
 if(!strcmp(status,"SERVICE_NOT_FOUND"))return "Core found no registered handler for the named service";
 if(!strcmp(status,"HANDLER_NOT_FOUND"))return "the registered service handler returned NOT_FOUND";
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
 if(!strcmp(status,"RETRIEVAL_SKIPPED"))return "no eligible factual corpus search candidate was selected";
 if(!strcmp(status,"NO_SEARCH_CANDIDATE"))return "no eligible search subject was identified; no retrieval failure established";
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
  snprintf(e.file,sizeof e.file,"core/core_dsd.c");
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

/* Select upstream decision evidence, never IRC's final delivery observation. */
static int evidence_rank(const digit_dsd_event_t *e){
 const char *v=e->status;
 if(!strcmp(e->module,"irc"))return 0;
 /* NOT_FOUND from a registered handler can mean an optional capability
  * declined a request. A later successful response outweighs the probe,
  * while an actual missing Core registration remains a real failure. */
 if(!strcmp(v,"HANDLER_NOT_FOUND"))return 15;
 if(!strcmp(v,"RETRIEVAL_SKIPPED")||!strcmp(v,"NO_SEARCH_CANDIDATE")||
    !strcmp(v,"REASONING_ABSTAIN"))return 5;
 if(!strcmp(v,"SERVICE_ERROR")||!strcmp(v,"SERVICE_NOT_FOUND")||
    !strcmp(v,"INVALID_STATE")||!strcmp(v,"QUERY_ERROR")||
    !strcmp(v,"RETRIEVAL_UNAVAILABLE"))
     return !strcmp(e->module,"core")?90:100;
 if(!strcmp(v,"NO_GENERATOR"))return 95;
 if(!strcmp(v,"EVIDENCE_NOT_RENDERABLE")||
    !strcmp(v,"INSUFFICIENT_EVIDENCE"))return 80;
 if(!strcmp(v,"RESPONSE_ABSTAIN"))return 30;
 if(!strcmp(v,"KNOWLEDGE_UNKNOWN")||!strcmp(v,"UNKNOWN_STATED")||
    !strcmp(v,"CLARIFICATION_REQUIRED")||!strcmp(v,"CLARIFICATION_REQUESTED")||
    !strcmp(v,"RETRIEVAL_FAILED")||!strcmp(v,"RETRIEVAL_FAILURE_STATED"))return 75;
 if(!strcmp(v,"QUERY_NO_MATCH")||
    !strcmp(v,"EVIDENCE_INSUFFICIENT"))return 65;
 if(!strcmp(v,"RESPONSE_COMPOSED")||
    !strcmp(v,"RESPONSE_RETURNED"))return 35;
 if(!strcmp(v,"QUERY_MATCH")||!strcmp(v,"EVIDENCE_AVAILABLE"))return 20;
 if(e->result)return 70;
 return 0;
}
static const char *evidence_assessment(const digit_dsd_event_t *e){
 const char *v=e->status;
 if(!strcmp(v,"KNOWLEDGE_UNKNOWN")||!strcmp(v,"UNKNOWN_STATED"))
  return "Adequate knowledge was not established; unknown is a valid result.";
 if(!strcmp(v,"CLARIFICATION_REQUIRED")||!strcmp(v,"CLARIFICATION_REQUESTED"))
  return "Intended meaning unresolved; clarification is appropriate.";
 if(!strcmp(v,"INSUFFICIENT_EVIDENCE")||!strcmp(v,"EVIDENCE_INSUFFICIENT"))
  return "Reasoning did not establish adequate evidence for an answer.";
 if(!strcmp(v,"EVIDENCE_NOT_RENDERABLE"))
  return "Conversation View rejected available evidence as unsuitable for output.";
 if(!strcmp(v,"RESPONSE_ABSTAIN"))
  return "Conversation withheld a response; underlying decision must be examined.";
 if(!strcmp(v,"RETRIEVAL_SKIPPED")||!strcmp(v,"NO_SEARCH_CANDIDATE"))
  return "No eligible factual search subject was selected; Corpus retrieval was not attempted, so a retrieval failure is not established.";
 if(!strcmp(v,"REASONING_ABSTAIN"))
  return "Reasoning abstained; the trace does not establish a retrieval failure.";
 if(!strcmp(v,"QUERY_NO_MATCH"))
  return "SQLite3 returned no matching records; retrieval completed without a match.";
 if(!strcmp(v,"RETRIEVAL_FAILED")||!strcmp(v,"RETRIEVAL_FAILURE_STATED")||
    !strcmp(v,"QUERY_ERROR")||!strcmp(v,"RETRIEVAL_UNAVAILABLE"))
  return "Evidence retrieval failed; absence of knowledge is not established.";
 if(!strcmp(v,"NO_GENERATOR"))return "Conversation has no general response generator.";
 if(!strcmp(v,"RESPONSE_COMPOSED")||!strcmp(v,"RESPONSE_RETURNED"))
  return "A response was produced; substantive answer quality remains unverified.";
 if(!strcmp(v,"SERVICE_NOT_FOUND"))
  return "Core registry lookup found no handler for the named service.";
 if(!strcmp(v,"HANDLER_NOT_FOUND"))
  return "A registered service handler returned NOT_FOUND; its internal cause is unverified.";
 if(!strcmp(v,"SERVICE_ERROR")||!strcmp(v,"INVALID_STATE"))
  return "The named upstream service reported a technical failure.";
 return "The reported module status is observed; deeper cause is unverified.";
}
/* DSD owns interpretation of the evidence; adapters only transport lines.
 * A missing trace stage is never a claim that the underlying work did not run. */
static stnlabz_module_result_t report_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used,void *ctx){
 const digit_dsd_report_request_t *q=request;
 digit_dsd_report_result_t *out=response;
 digit_dsd_result_t d={0};
 unsigned i,sql=0,conversation=0,no_reply=0,failure=0,interpreted=0,no_generator=0;
 const digit_dsd_event_t *last=NULL,*responsible=NULL;
 int best_rank=0;
 size_t n;
 (void)ctx;
 if(used)*used=0;
 if(!q||request_size!=sizeof *q||!response||
    response_size<sizeof *out||!used||!q->trace_id||
    !memchr(q->origin,0,sizeof q->origin))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(n=0;q->origin[n];n++)
   if((unsigned char)q->origin[n]<32||(unsigned char)q->origin[n]==127)
     return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!diagnose(q->trace_id,&d))return STNLABZ_MODULE_ERR_INVALID_STATE;
 memset(out,0,sizeof *out);
 out->trace_id=q->trace_id;
 if(!d.found){
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "DSD: Trace %llu is not retained; no diagnosis available.",
     (unsigned long long)q->trace_id);
   *used=sizeof *out;return STNLABZ_MODULE_OK;
 }
 snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
   "Originating Term: %.250s",q->origin[0]?q->origin:"origin unavailable");
 for(i=0;i<d.count;i++){
   const digit_dsd_event_t *e=&d.events[i];
   if(!strcmp(e->module,"sqlite3"))sql++;
   if(!strcmp(e->module,"conversation"))conversation++;
   if(!strcmp(e->operation,"digit.interpretation"))interpreted++;
   if(!strcmp(e->status,"NO_RESPONSE"))no_reply++;
   if(!strcmp(e->module,"conversation")&&!strcmp(e->status,"NO_GENERATOR"))no_generator++;
   if(e->result)failure++;
   if(strcmp(e->status,"TRACE_BOUND"))last=e;
   if(evidence_rank(e)>best_rank){best_rank=evidence_rank(e);responsible=e;}
 }
 if(last){
   char chain[210]={0};size_t len=0;
   for(i=0;i<d.count;i++){
     const digit_dsd_event_t *e=&d.events[i];
     int n;
     if(strcmp(e->status,"CALL_ENTER"))continue;
     n=snprintf(chain+len,sizeof chain-len,"%s%.45s",
       len?" -> ":"",e->operation);
     if(n<0||(size_t)n>=sizeof chain-len)break;
     len+=(size_t)n;
   }
   if(!len)snprintf(chain,sizeof chain,"No Core call-entry evidence");
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "Route: %.210s; recorded=%u, dropped=%u",chain,d.count,d.dropped);
 }else
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "Route: only trace binding observed; downstream route unknown.");
 snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
   "Module: %s",responsible?responsible->module:
   "undetermined (upstream evidence missing)");
 if(responsible)
  snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
    "Code: %s:%u (%s.%s), service=%s, status=%s",
    responsible->file,responsible->line,responsible->module,
    responsible->method,responsible->operation,responsible->status);
 else
  snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
    "Code: upstream source unverified; IRC records only delivery.");
 if(sql){
   const digit_dsd_event_t *e=NULL;
   for(i=0;i<d.count;i++)
     if(!strcmp(d.events[i].module,"sqlite3"))e=&d.events[i];
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "SQL: %s at %s:%u; status=%s; records=%u",
     e->operation,e->file,e->line,e->status,e->records);
 }else
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "SQL: No SQLite3 operation recorded for this request; SQL outcome unknown.");
 snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
   "Assessment: %s",responsible?evidence_assessment(responsible):
   "Upstream cause was not recorded; no module attribution is justified.");
 if(d.dropped)
   snprintf(out->lines[out->count++],DIGIT_DSD_REPORT_LINE_MAX,
     "Evidence incomplete: %u trace event(s) dropped.",d.dropped);
 *used=sizeof *out;
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

/* Core owns the diagnostic service and its request-local trace state.
 * Keep the existing public ABI while removing authority from the module. */
int core_dsd_handles(const char *name){
 return name&&(
  !strcmp(name,DIGIT_DSD_BIND_SERVICE)||
  !strcmp(name,DIGIT_DSD_CURRENT_SERVICE)||
  !strcmp(name,DIGIT_DSD_RECORD_SERVICE)||
  !strcmp(name,DIGIT_DSD_DIAGNOSE_SERVICE)||
  !strcmp(name,DIGIT_DSD_COMMAND_SERVICE)||
  !strcmp(name,DIGIT_DSD_REPORT_SERVICE));
}
stnlabz_module_result_t core_dsd_dispatch(const char *name,
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used){
 if(!name)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!strcmp(name,DIGIT_DSD_BIND_SERVICE))
  return bind_service(request,request_size,response,response_size,used,NULL);
 if(!strcmp(name,DIGIT_DSD_CURRENT_SERVICE))
  return current_service(request,request_size,response,response_size,used,NULL);
 if(!strcmp(name,DIGIT_DSD_RECORD_SERVICE))
  return record_service(request,request_size,response,response_size,used,NULL);
 if(!strcmp(name,DIGIT_DSD_DIAGNOSE_SERVICE))
  return diagnose_service(request,request_size,response,response_size,used,NULL);
 if(!strcmp(name,DIGIT_DSD_COMMAND_SERVICE))
  return command_service(request,request_size,response,response_size,used,NULL);
 if(!strcmp(name,DIGIT_DSD_REPORT_SERVICE))
  return report_service(request,request_size,response,response_size,used,NULL);
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
/* Capture the actual call entry before running a module callback. */
void core_dsd_service_begin(const char *service,unsigned line){
 digit_dsd_event_t event={0};
 if(!current_trace||!service)return;
 event.trace_id=current_trace;event.span_id=7;event.line=line;
 snprintf(event.module,sizeof event.module,"core");
 snprintf(event.method,sizeof event.method,"digit_service");
 snprintf(event.file,sizeof event.file,"core/digit.c");
 snprintf(event.operation,sizeof event.operation,"%s",service);
 snprintf(event.status,sizeof event.status,"CALL_ENTER");
 (void)record(&event);
}
/* Diagnostic evidence for every service call, not only opt-in modules. */
void core_dsd_service_result(const char *service,
 stnlabz_module_result_t rc,size_t produced,unsigned line){
 digit_dsd_event_t event={0};
 if(!current_trace||!service)return;
 event.trace_id=current_trace;event.span_id=8;event.line=line;
 event.result=rc==STNLABZ_MODULE_OK?0:1;
 event.records=produced>UINT32_MAX?UINT32_MAX:(uint32_t)produced;
 snprintf(event.module,sizeof event.module,"core");
 snprintf(event.method,sizeof event.method,"digit_service");
 snprintf(event.file,sizeof event.file,"core/digit.c");
 snprintf(event.operation,sizeof event.operation,"%s",service);
 snprintf(event.status,sizeof event.status,"%s",
    rc==STNLABZ_MODULE_ERR_NOT_FOUND?"HANDLER_NOT_FOUND":
    rc==STNLABZ_MODULE_ERR_INVALID_ARGUMENT?"INVALID_ARGUMENT":
    rc==STNLABZ_MODULE_ERR_INVALID_STATE?"INVALID_STATE":
    rc!=STNLABZ_MODULE_OK?"SERVICE_ERROR":
    produced?"RESPONSE_RETURNED":"NO_RESPONSE");
 (void)record(&event);
}

/* Core registry miss is distinct from a registered handler returning
 * NOT_FOUND. Keep these signals separate to avoid invented attribution. */
void core_dsd_service_missing(const char *service,unsigned line){
 digit_dsd_event_t event={0};
 if(!current_trace||!service)return;
 event.trace_id=current_trace;event.span_id=8;event.line=line;
 event.result=1;
 snprintf(event.module,sizeof event.module,"core");
 snprintf(event.method,sizeof event.method,"registry_lookup");
 snprintf(event.file,sizeof event.file,"core/digit.c");
 snprintf(event.operation,sizeof event.operation,"%s",service);
 snprintf(event.status,sizeof event.status,"SERVICE_NOT_FOUND");
 (void)record(&event);
}
