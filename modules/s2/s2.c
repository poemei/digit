/* [AI:GPT-6 | 2026-10-19] S2 bounded, passive reconnaissance adapter. */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "module.h"
#include "s2.h"
#include "s2_observations.h"
#define RICTUS "/var/log/rictus/rictus.log"
#define SENTINEL "https://api.stn-labz.com/threats"
#define FETCH_MAX (1024u*1024u)
#define RICTUS_LINES 128u
#define POLL_SECONDS 300
#define DELIVER_SERVICE "digit.irc.deliver"
typedef struct {char *data;size_t length;} buffer_t;
static const stnlabz_module_host_t *host;
static volatile int busy;
static pthread_t scheduler;
static pthread_mutex_t scheduler_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t scheduler_cond=PTHREAD_COND_INITIALIZER;
static int scheduler_stop,scheduler_started;
static time_t previous_poll;
static char previous_signature[192];
static long previous_lines;
static int has_reported;
static int last_source_state=-1;
static int64_t observation_cursor;
static void lock(void){while(__sync_lock_test_and_set(&busy,1)){}}
static void unlock(void){__sync_lock_release(&busy);}
static size_t collect(char *input,size_t size,size_t count,void *userdata){
 buffer_t *b=userdata;size_t n;
 if(size && count>SIZE_MAX/size)return 0;
 n=size*count;
 if(n>FETCH_MAX-b->length)return 0;
 memcpy(b->data+b->length,input,n);
 b->length+=n;b->data[b->length]=0;return n;
}
static int fetch(buffer_t *b){
 CURL *c=curl_easy_init();long status=0;CURLcode rc;
 if(!c)return 0;
 b->length=0;b->data=malloc(FETCH_MAX+1);
 if(!b->data){curl_easy_cleanup(c);return 0;}
 b->data[0]=0;
 curl_easy_setopt(c,CURLOPT_URL,SENTINEL);
 curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(c,CURLOPT_WRITEDATA,b);
 curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,3L);
 curl_easy_setopt(c,CURLOPT_TIMEOUT,8L);
 curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(c,CURLOPT_USERAGENT,"Digit-S2/0.1");
 rc=curl_easy_perform(c);
 if(rc==CURLE_OK)curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
 curl_easy_cleanup(c);
 if(rc!=CURLE_OK||status!=200){free(b->data);b->data=NULL;return 0;}
 return 1;
}
static int word_ip(const char *line,const char *ip){
 const char *p=line;size_t n;
 if(!ip||!*ip||strlen(ip)>45)return 0;
 n=strlen(ip);
 while((p=strstr(p,ip))!=NULL){
  if((p==line||(!isdigit((unsigned char)p[-1])&&p[-1]!='.')) &&
   (!isdigit((unsigned char)p[n])&&p[n]!='.'))return 1;
  p++;
 }
 return 0;
}
static const char *field(json_object *obj,const char *name){
 json_object *v;
 if(!obj||!json_object_is_type(obj,json_type_object)||
    !json_object_object_get_ex(obj,name,&v)||
    !json_object_is_type(v,json_type_string))return "";
 return json_object_get_string(v);
}
static int high(const char *severity,const char *relevance){
 return ((!strcmp(severity,"HIGH")||!strcmp(severity,"CRITICAL"))&&
         (!strcmp(relevance,"HIGH")||!strcmp(relevance,"CRITICAL")));
}
static long local_tail(char lines[RICTUS_LINES][512],size_t *stored){
 FILE *f=fopen(RICTUS,"r");char line[512];long count=0;
 *stored=0;
 if(!f)return -1;
 while(fgets(line,sizeof line,f)){
  size_t n=strlen(line);
  if(n&&!strchr(line,'\n')&&!feof(f)){
   int ch;while((ch=fgetc(f))!=EOF&&ch!='\n'){}
   continue; /* no partial records */
  }
  snprintf(lines[(size_t)count%RICTUS_LINES],512,"%s",line);
  count++;
 }
 fclose(f);*stored=(size_t)(count<RICTUS_LINES?count:RICTUS_LINES);
 return count;
}
static int exact_ip(const char *s){
 unsigned a,b,c,d;char extra;
 if(sscanf(s,"%u.%u.%u.%u%c",&a,&b,&c,&d,&extra)!=4)return 0;
 return a<=255&&b<=255&&c<=255&&d<=255;
}
static int match_lines(const char lines[RICTUS_LINES][512],long count,
 size_t stored,const char *ip,const char *other){
 size_t i;
 for(i=0;i<stored;i++){
  const char *line=lines[(size_t)(count-(long)stored+(long)i)%RICTUS_LINES];
  if((exact_ip(ip)&&word_ip(line,ip)) ||
     (exact_ip(other)&&word_ip(line,other)))return 1;
 }
 return 0;
}
static stnlabz_module_result_t poll_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context){
 digit_s2_report_t *out=response;buffer_t b={0};
 char lines[RICTUS_LINES][512];size_t stored=0;
 long count;int available,has_priority=0,matched=0,stale=0,source_state,first_report;
 json_object *root=NULL,*records=NULL,*entry;
 const char *severity,*relevance,*id,*first,*second,*date,*best_date="";
 char signature[192]={0};char report[DIGIT_S2_REPORT_CAP]={0};
 time_t now=time(NULL);
 size_t i,length;
 (void)request;(void)context;
 if(response_used)*response_used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(request_size||!out||response_size<sizeof *out||!response_used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(now==(time_t)-1)return STNLABZ_MODULE_ERR_INVALID_STATE;
 lock();
 if(previous_poll&&now>=previous_poll&&now-previous_poll<POLL_SECONDS){
  unlock();return STNLABZ_MODULE_OK;
 }
 previous_poll=now;
 unlock();
 count=local_tail(lines,&stored);
 available=fetch(&b);
 if(available)root=json_tokener_parse(b.data);
 if(root){
  if(json_object_is_type(root,json_type_array))records=root;
  else if(json_object_is_type(root,json_type_object))
   json_object_object_get_ex(root,"threats",&records);
  if(!records||!json_object_is_type(records,json_type_array))available=0;
 }else available=0;
 if(available){
  length=json_object_array_length(records);
  if(length>5000)length=5000;
  for(i=0;i<length;i++){
   entry=json_object_array_get_idx(records,(int)i);
   if(!entry||!json_object_is_type(entry,json_type_object))continue;
   severity=field(entry,"severity");relevance=field(entry,"relevance");
   if(!high(severity,relevance))continue;
   has_priority=1;id=field(entry,"id");date=field(entry,"created_at");
   first=field(entry,"ip");
   {json_object *details=NULL;
    const char *second_ip="";
    if(json_object_object_get_ex(entry,"details",&details))
     second_ip=field(details,"ip_address");
    if(count>=0&&match_lines(lines,count,stored,first,second_ip))matched=1;
   }
   if(strlen(date)==19&&strcmp(date,best_date)>0){
    best_date=date;
    snprintf(signature,sizeof signature,"%.90s:%.30s:%.25s:%.25s",
       id,date,severity,relevance);
   }
  }
 }
 source_state=(available?1:0)|(count>=0?2:0);
 lock();first_report=!has_reported;unlock();
 if(!available){
  snprintf(report,sizeof report,"[S2] Sentinel feed unavailable or unrecognized; Rictus %s. No all-clear.",
     count<0?"unavailable":"accessible");
 }else if(count<0){
  snprintf(report,sizeof report,"[S2] Rictus log unavailable; Sentinel feed read. Correlation incomplete.");
 }else if(has_priority){
  snprintf(report,sizeof report,
   "[S2] HIGH/CRITICAL severity and relevance observed in Sentinel; Rictus indicator match=%s; investigation pending (not proof of compromise).",
   matched?"yes":"not found");
 }else if(first_report){
  snprintf(report,sizeof report,
   "[S2] Mission online: Rictus accessible; Sentinel accessible; no HIGH/CRITICAL paired findings in this snapshot. No all-clear implied.");
 }else if(count>previous_lines){
  snprintf(report,sizeof report,
   "[S2] Rictus has %ld new log records in current file; Sentinel available; unclassified observations require review.",
   count-previous_lines);
 }else stale=1;
 if(!stale&&report[0]){
  lock();
  if(has_reported && source_state==last_source_state &&
     !strcmp(signature,previous_signature)&&count==previous_lines){
   unlock();goto done;
  }
  snprintf(previous_signature,sizeof previous_signature,"%s",signature);
  previous_lines=count;
  has_reported=1;
  last_source_state=source_state;
  unlock();
  memset(out,0,sizeof *out);
  snprintf(out->report,sizeof out->report,"%s",report);
  *response_used=sizeof *out;
 }else if(count>=0){
  lock();previous_lines=count;unlock();
 }
done:
 if(root)json_object_put(root);
 free(b.data);
 return STNLABZ_MODULE_OK;
}
static void test(stnlabz_module_qualification_result_t *r,int ok,int negative){
 r->tests_executed++;if(ok)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 size_t i;char lines[RICTUS_LINES][512]={{0}};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 test(r,high("HIGH","HIGH"),0);
 test(r,high("CRITICAL","HIGH"),0);
 test(r,high("HIGH","CRITICAL"),0);
 test(r,high("CRITICAL","CRITICAL"),0);
 test(r,!high("MEDIUM","MEDIUM"),1);
 test(r,!high("HIGH","LOW"),1);
 test(r,!high("LOW","HIGH"),1);
 test(r,!high("","HIGH"),1);
 test(r,exact_ip("108.178.43.142"),0);
 test(r,exact_ip("34.66.92.201"),0);
 test(r,!exact_ip("999.1.1.1"),1);
 test(r,!exact_ip("1.1.1.1x"),1);
 test(r,!exact_ip(""),1);
 strcpy(lines[0],"connection from 108.178.43.142");
 strcpy(lines[1],"other 34.66.92.201");
 test(r,match_lines((const char (*)[512])lines,2,2,"108.178.43.142",""),0);
 test(r,match_lines((const char (*)[512])lines,2,2,"","34.66.92.201"),0);
 test(r,!match_lines((const char (*)[512])lines,2,2,"1.1.1.1",""),1);
 for(i=0;i<10;i++){
  char ip[30];snprintf(ip,sizeof ip,"10.0.0.%zu",i);
  test(r,exact_ip(ip),0);
 }
 return r->tests_executed>=20&&!r->tests_failed&&r->negative_test_executed&&
   r->negative_test_passed?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
/* Passive, bounded observation bridge. Report contains a reference to
 * source evidence, not an independently verified threat assertion. */
static int observation_report(digit_s2_report_t *out,int64_t *selected){
 digit_s2_observations_query_t query={0};
 digit_s2_observations_result_t result={0};
 size_t used=0;
 const char *id;
 char token[48]={0};
 size_t i=0;
 if(!out||!selected||!host||!host->invoke_service)return 0;
 query.after_id=observation_cursor;
 if(host->invoke_service(DIGIT_S2_OBSERVATIONS_SERVICE,&query,sizeof query,
    &result,sizeof result,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof result||result.id<=query.after_id||
    !memchr(result.text,0,sizeof result.text))return 0;
 id=strstr(result.text,"INT-");
 if(id){
  while(id[i]&&i<sizeof token-1&&
        ((id[i]>='A'&&id[i]<='Z')||
         (id[i]>='0'&&id[i]<='9')||id[i]=='-')){
   token[i]=id[i];i++;
  }
 }
 if(!token[0])snprintf(token,sizeof token,"not supplied");
 memset(out,0,sizeof *out);
 snprintf(out->report,sizeof out->report,
  "[S2] IRC observation #%lld: Rictus Intel report %s captured; "
  "source unverified, follow-up pending.",(long long)result.id,token);
 *selected=result.id;
 return 1;
}
/* Dedicated scheduler: never invokes an ABI service from another service
 * handler, avoiding Core's non-recursive registry lock. */
static void *scheduler_main(void *unused){
 digit_s2_report_t result={0},pending={0};size_t used=0;
 int64_t pending_observation=0;
 struct timespec until;
 (void)unused;
 for(;;){
  int stopping_now;
  pthread_mutex_lock(&scheduler_mutex);
  stopping_now=scheduler_stop;
  pthread_mutex_unlock(&scheduler_mutex);
  if(stopping_now)break;
  if(!pending.report[0]&&observation_report(&pending,&pending_observation)){
   /* Observation delivery is prioritized; the existing Sentinel/Rictus
    * cycle resumes when the observation queue has been drained. */
  }
  if(!pending.report[0]){
   memset(&result,0,sizeof result);used=0;
   if(poll_service(NULL,0,&result,sizeof result,&used,NULL)==STNLABZ_MODULE_OK&&
      used==sizeof result&&memchr(result.report,0,sizeof result.report))
    pending=result;
  }
  if(pending.report[0]&&host&&host->invoke_service){
   size_t delivered=0;
   if(host->invoke_service(DELIVER_SERVICE,&pending,sizeof pending,
      NULL,0,&delivered)==STNLABZ_MODULE_OK){
    if(pending_observation){observation_cursor=pending_observation;pending_observation=0;}
    memset(&pending,0,sizeof pending);
   }
  }
  if(clock_gettime(CLOCK_REALTIME,&until)!=0)break;
  until.tv_sec+=pending.report[0]?5:POLL_SECONDS;
  pthread_mutex_lock(&scheduler_mutex);
  while(!scheduler_stop){
   int rc=pthread_cond_timedwait(&scheduler_cond,&scheduler_mutex,&until);
   if(rc==ETIMEDOUT||rc!=0)break;
  }
  stopping_now=scheduler_stop;
  pthread_mutex_unlock(&scheduler_mutex);
  if(stopping_now)break;
 }
 return NULL;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->register_service||!api->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 host=api;
 if(!api->register_service(DIGIT_S2_POLL_SERVICE,poll_service,NULL)){
  host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 observation_cursor=0;
 scheduler_stop=0;
 if(pthread_create(&scheduler,NULL,scheduler_main,NULL)){
  (void)api->unregister_service(DIGIT_S2_POLL_SERVICE,NULL);
  host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 scheduler_started=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host||!scheduler_started)return STNLABZ_MODULE_ERR_INVALID_STATE;
 pthread_mutex_lock(&scheduler_mutex);
 scheduler_stop=1;
 pthread_cond_signal(&scheduler_cond);
 pthread_mutex_unlock(&scheduler_mutex);
 if(pthread_join(scheduler,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 scheduler_started=0;
 if(!host->unregister_service(DIGIT_S2_POLL_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="s2",.name="S2 passive security intelligence",
 .version_major=0,.version_minor=1,.version_patch=3,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
