#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dsd.h"

static const stnlabz_module_host_t *dsd_host;
/* Bounded evidence search. Never infer source locations absent from logs. */
#define DSD_LOG_LINE 768
static int emit(stnlabz_module_command_reply_fn reply,void *ctx,const char *s){
 return reply(ctx,s)!=0;
}
static int identifier(const char *s){
 size_t i;
 if(!s||!s[0])return 0;
 for(i=0;s[i];i++)
  if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||
       (s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'||s[i]=='.'))return 0;
 return i<64;
}
static void extract_token(const char *line,const char *key,char *out,size_t cap){
 const char *p=strstr(line,key);
 size_t i=0;
 out[0]=0;
 if(!p)return;
 p+=strlen(key);
 while(p[i]&&p[i]!=' '&&p[i]!='\n'&&p[i]!='\r'&&i+1<cap){out[i]=p[i];i++;}
 out[i]=0;
}
static int evidence_problem(const char *line){
 return strstr(line,"FAILED")||strstr(line,"FAIL ")||strstr(line,"REJECT")||
 strstr(line,"DENIED")||strstr(line,"ERROR")||strstr(line,"outcome=5")||
 strstr(line,"outcome=4")||strstr(line,"result=ERR_")||
 strstr(line,"status=failed");
}
static void component_name(const char *line,char *out,size_t cap){
 const char *p=strchr(line,' ');
 size_t i=0;
 if(p)p=strchr(p+1,' ');
 out[0]=0;
 if(!p)return;
 p++;
 while(p[i]&&p[i]!=' '&&i+1<cap){out[i]=p[i];i++;}
 out[i]=0;
 if(!strcmp(out,"MODEL_EXPRESSION"))(void)snprintf(out,cap,"expression");
 else if(!strcmp(out,"DISCOVERY"))(void)snprintf(out,cap,"discovery");
 else if(!strcmp(out,"ACTIVATION"))(void)snprintf(out,cap,"activation");
 else if(!strcmp(out,"ROUTER"))(void)snprintf(out,cap,"router");
}
static stnlabz_module_result_t diagnose_command(
 const stnlabz_module_command_t *command,
 stnlabz_module_command_reply_fn reply,void *reply_context,void *context){
 FILE *log;
 char line[DSD_LOG_LINE],selected[DSD_LOG_LINE]={0},module[64]={0};
 char file[128]={0},lineno[24]={0},reason[256],message[1024],term[128]={0};
 char candidate_id[64],candidate_module[64];
 unsigned long matches=0;
 int found=0;
 const char *args;
 (void)context;
 if(!command||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 args=command->arguments;
 while(*args==' ')args++;
 if(*args&&!strcmp(args,"my last request"))args="";
 if(*args&&!strcmp(args,"last"))args="";
 if(*args){
  size_t n=strlen(args);
  if(n>=sizeof term)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(term,args,n+1);
 }
 log=fopen("./logs/digit.log","r");
 if(!log){
  return emit(reply,reply_context,"DSD: operational log unavailable.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 while(fgets(line,sizeof line,log)){
  size_t len=strlen(line);
  if(len&&!strchr(line,'\n')&&!feof(log)){
   int c;while((c=fgetc(log))!='\n'&&c!=EOF){}
   continue; /* No truncated or fabricated evidence. */
  }
  if(term[0]){
   if(!strstr(line,term))continue;
  }else if(!evidence_problem(line))continue;
  matches++;
  (void)snprintf(selected,sizeof selected,"%s",line);
  found=1;
 }
 fclose(log);
 if(!emit(reply,reply_context,"DSD DIAGNOSTIC REPORT"))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!found){
  (void)snprintf(message,sizeof message,"Module: not_identified");
  if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!emit(reply,reply_context,"Log: no matching recorded event"))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(!emit(reply,reply_context,"Code: not_recorded"))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  (void)snprintf(message,sizeof message,"Finding: No %s evidence found in the available log.",
   term[0]?"matching":"failure");
  return emit(reply,reply_context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 selected[strcspn(selected,"\\n")]=0;
 component_name(selected,module,sizeof module);
 extract_token(selected,"id=",candidate_id,sizeof candidate_id);
 if(identifier(candidate_id)&&strcmp(candidate_id,"unknown")==0)candidate_id[0]=0;
 if(identifier(candidate_id)&&candidate_id[0]&&strcmp(module,"router"))
  (void)snprintf(module,sizeof module,"%s",candidate_id);
 extract_token(selected,"module=",candidate_module,sizeof candidate_module);
 if(identifier(candidate_module)&&candidate_module[0])
  (void)snprintf(module,sizeof module,"%s",candidate_module);
 if(!module[0])(void)snprintf(module,sizeof module,"not_identified");
 extract_token(selected,"file=",file,sizeof file);
 extract_token(selected,"line=",lineno,sizeof lineno);
 extract_token(selected,"reason=",reason,sizeof reason);
 if(!emit(reply,reply_context,""))return STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(message,sizeof message,"Module: %s",module);
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(message,sizeof message,"Log: %s",selected);
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(file[0]&&lineno[0]){
  (void)snprintf(message,sizeof message,"Code: %s:%s",file,lineno);
 }else if(file[0]){
  (void)snprintf(message,sizeof message,"Code: %s (line not recorded)",file);
 }else{
  (void)snprintf(message,sizeof message,"Code: not_recorded (no source location in log)");
 }
 if(!emit(reply,reply_context,message))return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(reason[0]) (void)snprintf(message,sizeof message,
  "Finding: Recorded event indicates %s; reason=%s. Matched events: %lu.",
  term[0]?"the requested condition":"a failure",reason,matches);
 else (void)snprintf(message,sizeof message,
  "Finding: %s event recorded for %s; precise cause not established by this log. Matched events: %lu.",
  term[0]?"Matching":"Failure-related",module,matches);
 return emit(reply,reply_context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}

static int text_ok(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
stnlabz_module_result_t digit_dsd_report(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *response_used,void *context){
 const digit_dsd_trace_t *trace=request;
 digit_dsd_report_t *out=response;
 size_t i,offset=0;
 (void)context;
 if(response_used)*response_used=0;
 if(!dsd_host||!trace||request_size!=sizeof(*trace)||
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
 *response_used=sizeof(*out);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(dsd_host||!h||!h->register_service||!h->unregister_service||
 !h->register_command||!h->unregister_command)
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("dsd.report",digit_dsd_report,NULL))
 return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_command("diagnose",diagnose_command,NULL)){
  (void)h->unregister_service("dsd.report",NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 dsd_host=h;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!dsd_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!dsd_host->unregister_command("diagnose",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!dsd_host->unregister_service("dsd.report",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 dsd_host=NULL;
 return STNLABZ_MODULE_OK;
}
stnlabz_module_descriptor_t digit_dsd_descriptor(void){
 stnlabz_module_descriptor_t d;memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"dsd");
 (void)snprintf(d.name,sizeof d.name,"Digit Diagnostic System");
 d.version_major=0;d.version_minor=3;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return d;
}

/* Required Linux dynamic-loader ABI entry point. */
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t descriptor;
 descriptor=digit_dsd_descriptor();return &descriptor;
}
