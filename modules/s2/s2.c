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
#include <sqlite3.h>
#include "module.h"
#include "s2.h"
#include "s2_observations.h"
#define RICTUS "/var/log/rictus/rictus.log"
#define SENTINEL "https://api.stn-labz.com/threats"
#define FETCH_MAX (1024u*1024u)
#define RICTUS_LINES 128u
#define POLL_SECONDS 300
#define DELIVER_SERVICE "digit.irc.deliver"
#define INTEL_DELIVERY "digit.slack.intel.deliver"
#define INTEL_REPORT_CAP 8192
#define CASE_DB "/opt/digit/state/s2_cases.sqlite3"
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
static char intel_pending[INTEL_REPORT_CAP];
static pthread_mutex_t intel_mutex=PTHREAD_MUTEX_INITIALIZER;
static void intelligence_brief(char *,size_t,const char *,long,int,int,int,const char *,int,int,int);
static int deliver_intel(const char *);
static void irc_notice(digit_s2_report_t *,const char *);
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

/* MEDIUM begins investigation. A repeated API snapshot is not new evidence.
 * SQLite keeps per-source case state across S2 hotloads. */
static int rank_level(const char *s){
 if(!strcmp(s,"CRITICAL"))return 4;
 if(!strcmp(s,"HIGH"))return 3;
 if(!strcmp(s,"MEDIUM"))return 2;
 if(!strcmp(s,"LOW"))return 1;
 return 0;
}
static int investigation_candidate(const char *severity,const char *relevance){
 (void)relevance;
 return rank_level(severity)>=2;
}
static int case_db_open(sqlite3 **db){
 if(sqlite3_open_v2(CASE_DB,db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,NULL)!=SQLITE_OK){
  if(*db)sqlite3_close(*db);
  *db=NULL;return 0;
 }
 sqlite3_busy_timeout(*db,1000);
 const char *ddl=
  "CREATE TABLE IF NOT EXISTS s2_cases ("
  "source_id TEXT PRIMARY KEY, severity TEXT NOT NULL, relevance TEXT NOT NULL,"
  "source_time TEXT NOT NULL, primary_ip TEXT NOT NULL, detail_ip TEXT NOT NULL,"
  "domain TEXT NOT NULL, request_url TEXT NOT NULL, first_seen INTEGER NOT NULL,"
  "last_seen INTEGER NOT NULL, revisions INTEGER NOT NULL DEFAULT 1);"
  "CREATE TABLE IF NOT EXISTS s2_case_events ("
  "event_id INTEGER PRIMARY KEY AUTOINCREMENT,source_id TEXT NOT NULL,"
  "event_type TEXT NOT NULL,recorded_at INTEGER NOT NULL,"
  "source_time TEXT NOT NULL,severity TEXT NOT NULL,relevance TEXT NOT NULL,"
  "primary_ip TEXT NOT NULL,detail_ip TEXT NOT NULL,"
  "domain TEXT NOT NULL,request_url TEXT NOT NULL);"
  "CREATE INDEX IF NOT EXISTS idx_s2_case_events_source "
  "ON s2_case_events(source_id,event_id);";
 if(sqlite3_exec(*db,ddl,NULL,NULL,NULL)!=SQLITE_OK){
  sqlite3_close(*db);*db=NULL;return 0;
 }
 return 1;
}
static int case_changed(const char *const old[7],const char *const current[7]){
 for(int i=0;i<7;i++)if(strcmp(old[i]?old[i]:"",current[i]?current[i]:""))return 1;
 return 0;
}
static const char *case_transition(int prior_severity,int prior_relevance,
 int severity,int relevance,int changed){
 if(prior_severity<0)return "OPENED";
 if(!changed)return NULL;
 if(severity>prior_severity||relevance>prior_relevance)return "ESCALATED";
 if(severity<prior_severity||relevance<prior_relevance)return "DECREASED";
 return "EVIDENCE_UPDATED";
}
static int update_case(sqlite3 *db,json_object *entry,char *event,size_t cap){
 const char *id=field(entry,"id"),*severity=field(entry,"severity"),
  *relevance=field(entry,"relevance"),*when=field(entry,"created_at"),
  *primary=field(entry,"ip");
 json_object *details=NULL;
 json_object_object_get_ex(entry,"details",&details);
 const char *other=field(details,"ip_address"),*domain=field(details,"domain"),
  *path=field(details,"request_url");
 if(!investigation_candidate(severity,relevance)||!id[0]||strlen(id)>120||
    !when[0]||strlen(when)>63)return 0;
 const char *values[]={severity,relevance,when,primary,other,domain,path};
 int previous_severity=-1,previous_relevance=-1,changed=1,rc=-1;
 sqlite3_stmt *st=NULL;
 if(sqlite3_prepare_v2(db,
   "SELECT severity,relevance,source_time,primary_ip,detail_ip,domain,request_url "
   "FROM s2_cases WHERE source_id=?1",-1,&st,NULL)!=SQLITE_OK)return -1;
 sqlite3_bind_text(st,1,id,-1,SQLITE_TRANSIENT);
 int step=sqlite3_step(st);
 if(step==SQLITE_ROW){
  const char *old[7];
  for(int i=0;i<7;i++){
   old[i]=(const char*)sqlite3_column_text(st,i);
  }
  previous_severity=rank_level(old[0]?old[0]:"");
  previous_relevance=rank_level(old[1]?old[1]:"");
  changed=case_changed(old,values);
 }else if(step!=SQLITE_DONE){sqlite3_finalize(st);return -1;}
 sqlite3_finalize(st);
 const char *transition=case_transition(previous_severity,previous_relevance,
  rank_level(severity),rank_level(relevance),changed);
 if(!transition)return 0;
 /* Both the case snapshot and its evidence history commit together. */
 if(sqlite3_exec(db,"SAVEPOINT s2_case_write",NULL,NULL,NULL)!=SQLITE_OK)return -1;
 const char *sql="INSERT INTO s2_cases "
 "(source_id,severity,relevance,source_time,primary_ip,detail_ip,domain,"
 "request_url,first_seen,last_seen,revisions) VALUES "
 "(?1,?2,?3,?4,?5,?6,?7,?8,strftime('%s','now'),strftime('%s','now'),1) "
 "ON CONFLICT(source_id) DO UPDATE SET "
 "severity=excluded.severity,relevance=excluded.relevance,"
 "source_time=excluded.source_time,primary_ip=excluded.primary_ip,"
 "detail_ip=excluded.detail_ip,domain=excluded.domain,"
 "request_url=excluded.request_url,last_seen=excluded.last_seen,"
 "revisions=s2_cases.revisions+1";
 if(sqlite3_prepare_v2(db,sql,-1,&st,NULL)!=SQLITE_OK)goto rollback;
 sqlite3_bind_text(st,1,id,-1,SQLITE_TRANSIENT);
 for(int i=0;i<7;i++)sqlite3_bind_text(st,i+2,values[i],-1,SQLITE_TRANSIENT);
 step=sqlite3_step(st);sqlite3_finalize(st);st=NULL;
 if(step!=SQLITE_DONE)goto rollback;
 if(sqlite3_prepare_v2(db,
   "INSERT INTO s2_case_events(source_id,event_type,recorded_at,source_time,"
   "severity,relevance,primary_ip,detail_ip,domain,request_url) "
   "VALUES(?1,?2,strftime('%s','now'),?3,?4,?5,?6,?7,?8,?9)",
   -1,&st,NULL)!=SQLITE_OK)goto rollback;
 const char *audit[]={id,transition,when,severity,relevance,primary,other,domain,path};
 for(int i=0;i<9;i++)sqlite3_bind_text(st,i+1,audit[i],-1,SQLITE_TRANSIENT);
 step=sqlite3_step(st);sqlite3_finalize(st);st=NULL;
 if(step!=SQLITE_DONE)goto rollback;
 if(sqlite3_exec(db,"RELEASE s2_case_write",NULL,NULL,NULL)!=SQLITE_OK)goto rollback;
 snprintf(event,cap,"%s id=%.90s severity=%s relevance=%s source_time=%.40s "
  "primary_ip=%.45s details_ip=%.45s domain=%.80s request=%.100s",
  transition,id,severity,relevance,when,primary,other,domain,path);
 return 1;
rollback:
 if(st)sqlite3_finalize(st);
 rc=sqlite3_exec(db,"ROLLBACK TO s2_case_write; RELEASE s2_case_write",NULL,NULL,NULL);
 (void)rc;return -1;
}
static int ingest_cases(json_object *records,char *summary,size_t cap,int *total){
 sqlite3 *db=NULL;
 if(!case_db_open(&db))return -1;
 int updates=0,errors=0;size_t pos=0;
 size_t length=json_object_array_length(records);
 if(length>5000)length=5000;
 for(size_t i=0;i<length;i++){
  json_object *entry=json_object_array_get_idx(records,(int)i);
  if(!entry||!json_object_is_type(entry,json_type_object))continue;
  char event[600]={0};
  int rc=update_case(db,entry,event,sizeof event);
  if(rc<0){errors++;continue;}
  if(rc>0){
   updates++;
   if(updates<=5&&pos<cap){
    int n=snprintf(summary+pos,cap-pos,"%s\n",event);
    if(n>0&&(size_t)n<cap-pos)pos+=(size_t)n;
   }
  }
 }
 sqlite3_stmt *st=NULL;*total=-1;
 if(sqlite3_prepare_v2(db,"SELECT count(*) FROM s2_cases",-1,&st,NULL)==SQLITE_OK){
  if(sqlite3_step(st)==SQLITE_ROW)*total=sqlite3_column_int(st,0);
  sqlite3_finalize(st);
 }
 sqlite3_close(db);
 return errors?-1:updates;
}
/* Read stored evidence only; ordering is priority then recorded recency,
 * not a claim that historical cases are recent attacks. */
/* Report stored case evidence, not inferred actor identities or attacks.
 * Recent means source_time is within the last 24 hours only when the
 * source timestamp can be parsed as UTC; ambiguous times are historical. */
/* Compact lead groups from stored API fields. A common field value is not
 * proof of a common actor. Source timestamps have unverified timezone. */
/* Case reporting is descriptive, not attribution. Each record belongs to
 * exactly one display group; repeated field values are not verified actors. */
static void case_investigation_snapshot(char *dst,size_t cap){
 sqlite3 *db=NULL;sqlite3_stmt *st=NULL;
 char cutoff[32]={0};struct tm utc={0};time_t now=time(NULL);
 size_t pos=0;
 if(!dst||!cap)return;
 dst[0]=0;
 if(now==(time_t)-1||!gmtime_r(&(time_t){now-86400},&utc)||
    !strftime(cutoff,sizeof cutoff,"%Y-%m-%d %H:%M:%S",&utc)){
  snprintf(dst,cap,"Source-time selection unavailable.\n");return;
 }
 if(!case_db_open(&db)){
  snprintf(dst,cap,"Case evidence unavailable (database read failed).\n");return;
 }
 const char *sql=
  "SELECT CASE WHEN detail_ip<>'' THEN 'details.ip_address' "
  "WHEN domain<>'' THEN 'domain' ELSE 'top-level ip' END AS field,"
  "CASE WHEN detail_ip<>'' THEN detail_ip WHEN domain<>'' THEN domain "
  "ELSE primary_ip END AS indicator,COUNT(*) AS records,"
  "MAX(CASE severity WHEN 'CRITICAL' THEN 4 WHEN 'HIGH' THEN 3 "
  "WHEN 'MEDIUM' THEN 2 ELSE 0 END) AS priority,"
  "MIN(source_time),MAX(source_time),MIN(source_id),MAX(source_id),"
  "SUM(CASE WHEN revisions>1 THEN 1 ELSE 0 END),"
  "MAX(CASE WHEN domain<>'' THEN domain ELSE '' END),"
  "MAX(CASE WHEN request_url<>'' THEN request_url ELSE '' END) "
  "FROM s2_cases WHERE source_time<>'' AND "
  "((?1=1 AND source_time>=?2) OR (?1=0 AND source_time<?2)) "
  "GROUP BY field,indicator ORDER BY "
  "CASE WHEN COUNT(*)>=2 THEN 0 ELSE 1 END,"
  "priority DESC,records DESC,MAX(source_time) DESC LIMIT 4";
 for(int recent=1;recent>=0;recent--){
  int n=snprintf(dst+pos,cap-pos,"\n*%s*\n\n",
    recent?"RECENT FINDINGS (source-time within provisional 24h)":
           "HISTORICAL FINDINGS (source-time older than provisional 24h)");
  if(n<=0||(size_t)n>=cap-pos)break;
  pos+=(size_t)n;
  if(sqlite3_prepare_v2(db,sql,-1,&st,NULL)!=SQLITE_OK){
   n=snprintf(dst+pos,cap-pos,"Case query unavailable.\n");
   if(n>0&&(size_t)n<cap-pos)pos+=(size_t)n;
   continue;
  }
  sqlite3_bind_int(st,1,recent);
  sqlite3_bind_text(st,2,cutoff,-1,SQLITE_TRANSIENT);
  int rc,rows=0;
  while((rc=sqlite3_step(st))==SQLITE_ROW){
   const char *v[11];
   for(int k=0;k<11;k++){
    v[k]=(const char*)sqlite3_column_text(st,k);
    if(!v[k]||!v[k][0])v[k]="not supplied";
   }
   int count=sqlite3_column_int(st,2),rank=sqlite3_column_int(st,3);
   int changes=sqlite3_column_int(st,8);
   const char *sev=rank==4?"CRITICAL":rank==3?"HIGH":rank==2?"MEDIUM":"UNKNOWN";
   const char *type=count>=2?"SHARED-VALUE GROUP (unverified)":"INDIVIDUAL FINDING";
   const char *action=!strcmp(v[0],"details.ip_address")?
    "Confirm details.ip_address role, then inspect source-specific logs.":
    !strcmp(v[0],"domain")?
    "Compare request paths and source evidence for this domain.":
    "Establish the top-level IP field's role before correlating actors.";
   n=snprintf(dst+pos,cap-pos,
    "*%s %d — %.22s: %.55s*\n\n"
    "Records: %d | Highest severity: %s | Revised records: %d\n\n"
    "Source times (timezone unverified): %.25s — %.25s\n\n"
    "Example IDs: %.48s%s%.48s\n\n"
    "Domain example: %.65s | Request example: %.80s\n\n"
    "Finding: %s\n\n"
    "Next: %s\n\n",
    type,++rows,v[0],v[1],count,sev,changes,
    v[4],v[5],v[6],strcmp(v[6],v[7])?", ":"",
    strcmp(v[6],v[7])?v[7]:"",
    v[9],v[10],
    count>=2?"Records share this source field value; field role, actor and compromise remain unverified.":
             "One qualifying API record; no same-value group established.",
    action);
   if(n<=0||(size_t)n>=cap-pos)break;
   pos+=(size_t)n;
  }
  if(!rows){
   n=snprintf(dst+pos,cap-pos,"No source records selected.\n\n");
   if(n>0&&(size_t)n<cap-pos)pos+=(size_t)n;
  }
  if(rc!=SQLITE_DONE&&rc!=SQLITE_ROW&&pos+50<cap){
   n=snprintf(dst+pos,cap-pos,"Evidence query incomplete.\n");
   if(n>0&&(size_t)n<cap-pos)pos+=(size_t)n;
  }
  sqlite3_finalize(st);st=NULL;
 }
 sqlite3_close(db);
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
 int case_updates=0,case_total=-1;char case_summary[3500]={0};
 json_object *root=NULL,*records=NULL,*entry;
 const char *severity,*relevance,*id,*first,*date,*best_date="";
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
  case_updates=ingest_cases(records,case_summary,sizeof case_summary,&case_total);
  length=json_object_array_length(records);
  if(length>5000)length=5000;
  for(i=0;i<length;i++){
   entry=json_object_array_get_idx(records,(int)i);
   if(!entry||!json_object_is_type(entry,json_type_object))continue;
   severity=field(entry,"severity");relevance=field(entry,"relevance");
   if(!investigation_candidate(severity,relevance))continue;
   has_priority=1;id=field(entry,"id");date=field(entry,"created_at");
   first=field(entry,"ip");
   {json_object *details=NULL;
    const char *second_ip="";
    if(json_object_object_get_ex(entry,"details",&details))
     second_ip=field(details,"ip_address");
    if(count>=0&&match_lines((const char (*)[512])lines,count,stored,first,second_ip))matched=1;
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
   "[S2] API severity MEDIUM/HIGH/CRITICAL under investigation; Rictus indicator match=%s (not proof of compromise).",
   matched?"yes":"not found");
 }else if(first_report){
  snprintf(report,sizeof report,
   "[S2] Mission online: Rictus accessible; Sentinel accessible; no MEDIUM-or-higher severity findings in this snapshot. No all-clear implied.");
 }else if(count>previous_lines){
  snprintf(report,sizeof report,
   "[S2] Rictus has %ld new log records in current file; Sentinel available; unclassified observations require review.",
   count-previous_lines);
 }else stale=1;
 if(!stale&&report[0]){
  lock();
  if(case_updates==0 && has_reported && source_state==last_source_state &&
     !strcmp(signature,previous_signature)&&count==previous_lines){
   unlock();goto done;
  }
  snprintf(previous_signature,sizeof previous_signature,"%s",signature);
  previous_lines=count;
  has_reported=1;
  last_source_state=source_state;
  unlock();
  char brief[INTEL_REPORT_CAP]={0};
  intelligence_brief(brief,sizeof brief,report,count,available,has_priority,matched,
    signature,!first_report,case_total,case_updates);
  {char findings[3600]={0};
   case_investigation_snapshot(findings,sizeof findings);
   size_t used=strlen(brief);
   if(used+strlen(findings)+160<sizeof brief)
    snprintf(brief+used,sizeof brief-used,
     "\n\n*INVESTIGATION FINDINGS* (individual versus shared-value groups; source timestamps provisional)\n%s",findings);
  }
  if(case_updates>0){
   size_t used=strlen(brief);
   snprintf(brief+used,sizeof brief-used,"\n\n*CASE ACTIVITY* (records opened/changed: %d; tracked cases: %d)\n%.3000s",
     case_updates,case_total,case_summary);
  }else if(case_updates<0){
   size_t used=strlen(brief);
   snprintf(brief+used,sizeof brief-used,"\n\n*CASE PERSISTENCE WARNING*: local case database failed; do not treat this as a complete investigation history.");
  }
  pthread_mutex_lock(&intel_mutex);
  /* Never overwrite an undelivered private brief. */
  if(!intel_pending[0])snprintf(intel_pending,sizeof intel_pending,"%s",brief);
  pthread_mutex_unlock(&intel_mutex);
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
 test(r,investigation_candidate("MEDIUM","MEDIUM"),0);
 {
  const char *old_fields[]={"MEDIUM","MEDIUM","2026-10-22 10:00:00","1.2.3.4","5.6.7.8","example.org","/login"};
  const char *same_fields[]={"MEDIUM","MEDIUM","2026-10-22 10:00:00","1.2.3.4","5.6.7.8","example.org","/login"};
  const char *new_path[]={"MEDIUM","MEDIUM","2026-10-22 10:00:00","1.2.3.4","5.6.7.8","example.org","/admin"};
  test(r,!case_changed(old_fields,same_fields),1);
  test(r,case_changed(old_fields,new_path),0);
  test(r,!strcmp(case_transition(-1,-1,2,2,1),"OPENED"),0);
  test(r,!strcmp(case_transition(2,2,3,2,1),"ESCALATED"),0);
  test(r,!strcmp(case_transition(3,3,2,3,1),"DECREASED"),0);
  test(r,!strcmp(case_transition(2,2,2,2,1),"EVIDENCE_UPDATED"),0);
  test(r,case_transition(2,2,2,2,0)==NULL,1);
 }

 test(r,investigation_candidate("HIGH","MEDIUM"),0);
 test(r,!investigation_candidate("LOW","HIGH"),1);
 test(r,investigation_candidate("MEDIUM","LOW"),0);
 test(r,investigation_candidate("HIGH","LOW"),0);
 test(r,investigation_candidate("CRITICAL",""),0);
 test(r,!investigation_candidate("LOW","CRITICAL"),1);
 test(r,!investigation_candidate("","HIGH"),1);
 test(r,rank_level("CRITICAL")>rank_level("HIGH"),0);
 test(r,!high("HIGH","LOW"),1);
 test(r,!high("LOW","HIGH"),1);
 test(r,!high("","HIGH"),1);
 test(r,exact_ip("108.178.43.142"),0);
 test(r,exact_ip("34.66.92.201"),0);
 test(r,strcmp("ip","ip_address")!=0,0);
 test(r,!exact_ip("999.1.1.1"),1);
 test(r,!exact_ip("1.1.1.1x"),1);
 test(r,!exact_ip(""),1);
 {
  char brief[INTEL_REPORT_CAP]={0};
  intelligence_brief(brief,sizeof brief,"Source event",-1,0,0,0,"",0,12,3);
  test(r,strstr(brief,"1. EXECUTIVE SUMMARY")!=NULL,0);
  test(r,strstr(brief,"6. FINAL ASSESSMENT")!=NULL,0);
  test(r,strstr(brief,"not yet established")!=NULL,0);
  test(r,strstr(brief,"Rictus: unavailable")!=NULL,0);
  test(r,strstr(brief,"12 source-record cases")!=NULL,0);
  test(r,strstr(brief,"3 source records opened or changed")!=NULL,0);
  {
   digit_s2_report_t notice={0};
   irc_notice(&notice,"MEDIUM investigation opened");
   test(r,strstr(notice.report,"MEDIUM investigation opened")!=NULL,0);
   test(r,strstr(notice.report,"Slack -> #stn-labz")!=NULL,0);
   test(r,strlen(notice.report)<sizeof notice.report,0);
  }
 }
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
/* Passive, bounded observation bridge. Rictus Intel is presented as
 * STN-Labz S2 intelligence transported through IRC. The current SQLite3
 * query does not authenticate sender identity; this label must not be used
 * as proof that the agent or an external claim was independently verified. */
/* Structured private assessment. Unknown counts are not invented.
 * Only source state, bounded log counts and direct indicator correlations
 * evidenced by this poll are reported. No raw log entries leave S2. */
static void intelligence_brief(char *dst,size_t cap,const char *short_report,
 long rictus_count,int sentinel_ok,int priority,int correlated,
 const char *signature,int is_update,int case_total,int case_updates){
 char stamp[40]="unknown",cases[128],changes[128];time_t now=time(NULL);
 struct tm utc={0};
 if(now!=(time_t)-1&&gmtime_r(&now,&utc))
  strftime(stamp,sizeof stamp,"%Y-%m-%d %H:%M UTC",&utc);
 if(case_total>=0)snprintf(cases,sizeof cases,"%d source-record cases",case_total);
 else snprintf(cases,sizeof cases,"unavailable (case database not read)");
 if(case_updates>=0)snprintf(changes,sizeof changes,"%d source records opened or changed",case_updates);
 else snprintf(changes,sizeof changes,"case persistence unavailable");
 snprintf(dst,cap,
  "STN-LABZ | SECURITY INTELLIGENCE\n\n"
  "*S2 INTELLIGENCE %s*\n\n"
  "Report generated: %s\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*1. EXECUTIVE SUMMARY*\n\n"
  "%s\n\n"
  "Rictus: %s | Sentinel: %s\n\n"
  "Case register: %s. This poll: %s.\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*2. ACTIVE INVESTIGATIONS*\n\n"
  "Tracked: %s.\n\n"
  "Latest ingestion: %s.\n\n"
  "Cases are keyed by Sentinel source ID. Evidence changes are recorded "
  "in a durable case-event history; severity/relevance changes never "
  "prove an incident or close an investigation. "
  "Initial bulk ingestion is historical import, not newly occurring attacks.\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*3. THREAT INTELLIGENCE OVERVIEW*\n\n"
  "Investigation threshold: API severity MEDIUM, HIGH or CRITICAL (relevance is context, not a gate).\n"
  "Qualifying source findings in this feed snapshot: %s.\n\n"
  "Rictus log records in current file: %ld.\n\n"
  "Higher severity requires explicit source evidence; historical "
  "24-hour rates and incident counts are not yet established.\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*4. CROSS-SOURCE INTELLIGENCE*\n\n"
  "Exact IPv4 overlap with the last 128 Rictus log records: %s.\n\n"
  "Source record signature: %.180s.\n\n"
  "An indicator match does not independently prove compromise; "
  "primary and details IPs retain separate roles.\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*5. INTELLIGENCE GAPS*\n\n"
  "Rictus event grammar, identity and role of IP fields, "
  "24-hour source windows, independent attribution and confirmed "
  "case outcomes and independent incident confirmation remain unresolved.\n\n"
  "━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n"
  "*6. FINAL ASSESSMENT*\n\n"
  "Investigate MEDIUM immediately, retain source evidence, and "
  "escalate only when corroborating records or severity changes support it. "
  "Neither this snapshot nor an IP overlap alone establishes compromise.",
  is_update?"INVESTIGATIVE UPDATE":"BRIEF",stamp,short_report,
  rictus_count<0?"unavailable":"accessible",
  sentinel_ok?"accessible":"unavailable or unrecognized",
  cases,changes,cases,changes,
  priority?"observed":"not observed",rictus_count,
  correlated?"found":"not found in inspected records",
  signature&&*signature?signature:"none");
}
static int deliver_intel(const char *brief){
 size_t used=0;
 if(!host||!host->invoke_service||!brief||!brief[0])return 0;
 size_t bytes=strlen(brief)+1;
 if(bytes>INTEL_REPORT_CAP)return 0;
 return host->invoke_service(INTEL_DELIVERY,brief,bytes,NULL,0,&used)==STNLABZ_MODULE_OK;
}
static int observation_report(digit_s2_report_t *out,int64_t *selected){
 digit_s2_observations_query_t query={0};
 digit_s2_observations_result_t result={0};
 size_t used=0;
 const char *id;
 char token[48]={0},summary[140]={0};
 const char *description;
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
 description=strstr(result.text,"SUMMARY:");
 description=description?description+8:result.text;
 while(*description==' ')description++;
 for(i=0;i<sizeof summary-1&&description[i]&&description[i]!='\n'&&description[i]!='\r';i++)
  summary[i]=description[i];
 memset(out,0,sizeof *out);
 snprintf(out->report,sizeof out->report,
  "[S2] IRC observation #%lld, Rictus Intel %s: %.130s "
  "(STN-Labz S2 intelligence; IRC transport).",
  (long long)result.id,token,summary);
 *selected=result.id;
 return 1;
}
/* Dedicated scheduler: never invokes an ABI service from another service
 * handler, avoiding Core's non-recursive registry lock. */
static void irc_notice(digit_s2_report_t *dst,const char *summary){
 if(!dst)return;
 memset(dst,0,sizeof *dst);
 snprintf(dst->report,sizeof dst->report,
  "[S2] %.255s | Full intelligence brief: Slack -> #stn-labz",
  summary&&*summary?summary:"Intelligence report published");
}
static void *scheduler_main(void *unused){
 digit_s2_report_t result={0},pending={0},poll_snapshot={0};size_t used=0;
 int64_t pending_observation=0;
 int drain_observations=0;
 int poll_ready=0;
 struct timespec until;
 (void)unused;
 for(;;){
  int stopping_now;
  pthread_mutex_lock(&scheduler_mutex);
  stopping_now=scheduler_stop;
  pthread_mutex_unlock(&scheduler_mutex);
  if(stopping_now)break;
  if(!pending.report[0]&&poll_ready){
   pending=poll_snapshot;
   memset(&poll_snapshot,0,sizeof poll_snapshot);
   poll_ready=0;
  }
  if(!pending.report[0]&&observation_report(&pending,&pending_observation)){
   /* Send the selected Rictus-linked observation as an investigative
    * update to private Slack. Do not advance its cursor on failure. */
   char update[INTEL_REPORT_CAP]={0};
   intelligence_brief(update,sizeof update,pending.report,-1,0,0,0,
     "S2 observation ID in summary",1,-1,-1);
   if(!deliver_intel(update)){
    memset(&pending,0,sizeof pending);
    pending_observation=0;
   }else{
    digit_s2_report_t notice={0};
    irc_notice(&notice,pending.report);
    pending=notice;
   }
  }
  if(!pending.report[0]){
   memset(&result,0,sizeof result);used=0;
   if(!poll_snapshot.report[0]&&
      poll_service(NULL,0,&result,sizeof result,&used,NULL)==STNLABZ_MODULE_OK&&
      used==sizeof result&&memchr(result.report,0,sizeof result.report))
    irc_notice(&poll_snapshot,result.report);
  }
  /* Deliver the full source-backed assessment only to the configured private
   * Slack intelligence channel, never to an IRC bridge or public website. */
  {
   char brief[INTEL_REPORT_CAP]={0};
   pthread_mutex_lock(&intel_mutex);
   if(intel_pending[0])memcpy(brief,intel_pending,sizeof brief);
   pthread_mutex_unlock(&intel_mutex);
   if(brief[0]&&deliver_intel(brief)){
    pthread_mutex_lock(&intel_mutex);
    if(!strcmp(intel_pending,brief))intel_pending[0]=0;
    pthread_mutex_unlock(&intel_mutex);
    if(poll_snapshot.report[0])poll_ready=1;
   }
  }
  if(pending.report[0]&&host&&host->invoke_service){
   size_t delivered=0;
   if(host->invoke_service(DELIVER_SERVICE,&pending,sizeof pending,
      NULL,0,&delivered)==STNLABZ_MODULE_OK){
    if(pending_observation){observation_cursor=pending_observation;pending_observation=0;drain_observations=1;}
    memset(&pending,0,sizeof pending);
   }
  }
  if(clock_gettime(CLOCK_REALTIME,&until)!=0)break;
  until.tv_sec+=pending.report[0]||drain_observations?5:POLL_SECONDS;
  drain_observations=0;
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
 pthread_mutex_lock(&intel_mutex);intel_pending[0]=0;pthread_mutex_unlock(&intel_mutex);
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
 .version_major=0,.version_minor=2,.version_patch=8,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
