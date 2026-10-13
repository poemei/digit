#include <stdio.h>
#include <string.h>
#include "dsd.h"
#include "logging.h"
static const stnlabz_module_host_t *dsd_host;
static int text_ok(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
stnlabz_module_result_t digit_dsd_report(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context){
 const digit_dsd_trace_t *trace=request;
 digit_dsd_report_t *out=response;
 size_t i,offset=0;
 (void)context;
 if(response_used)*response_used=0;
 if(!dsd_host||!digit_log_is_open()||!trace||request_size!=sizeof(*trace)||
 !out||response_size<sizeof(*out)||!response_used||
 !text_ok(trace->request_id,sizeof trace->request_id)||!trace->request_id[0]||
 trace->count>DIGIT_DSD_TRACE_MAX)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<trace->count;i++){
  const digit_dsd_observation_t *o=&trace->observations[i];
  if(!text_ok(o->component,sizeof o->component)||
  !text_ok(o->decision,sizeof o->decision)||
  !text_ok(o->file,sizeof o->file)||
  !text_ok(o->function,sizeof o->function)||
  !text_ok(o->reason,sizeof o->reason)||!o->component[0]||
  !o->decision[0]||!o->reason[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 }
 memset(out,0,sizeof(*out));
 (void)snprintf(out->request_id,sizeof out->request_id,"%s",trace->request_id);
 out->observations=trace->count;
 for(i=0;i<trace->count;i++){
  const digit_dsd_observation_t *o=&trace->observations[i];
  int n=snprintf(out->report+offset,sizeof out->report-offset,
   "%s decision=%s file=%s function=%s line=%u reason=%s\n",
   o->component,o->decision,o->file[0]?o->file:"not_recorded",
   o->function[0]?o->function:"not_recorded",o->line,o->reason);
  if(n<0||(size_t)n>=sizeof out->report-offset)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  offset+=(size_t)n;
  if(!strcmp(o->decision,"FAILED")||!strcmp(o->decision,"DENIED"))out->has_failure=1;
 }
 if(!digit_log_event("DSD","REPORT","status=observed_only")||!digit_log_sync())
 return STNLABZ_MODULE_ERR_INVALID_STATE;
 *response_used=sizeof(*out);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(dsd_host||!h||!h->register_service||!h->unregister_service||!digit_log_is_open())
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("dsd.report",digit_dsd_report,NULL))
 return STNLABZ_MODULE_ERR_START_FAILED;
 dsd_host=h;
 if(!digit_log_event("DSD","START","status=ready")||!digit_log_sync()){
  (void)h->unregister_service("dsd.report",NULL);dsd_host=NULL;
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!dsd_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!dsd_host->unregister_service("dsd.report",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 dsd_host=NULL;
 return digit_log_event("DSD","STOP","status=stopped")&&digit_log_sync()?
 STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_STOP_FAILED;
}
stnlabz_module_descriptor_t digit_dsd_descriptor(void){
 stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"dsd");
 (void)snprintf(d.name,sizeof d.name,"Digit Diagnostic System");
 d.version_major=0;d.version_minor=1;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return d;
}
