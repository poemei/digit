/* SQLite3-owned import watcher. Designated source files are operator-approved. */
#define _POSIX_C_SOURCE 200809L
#include "ingest.h"
#include <sqlite3.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <syslog.h>

#define DB "/opt/digit/data/digit_memory.db"
#define CORPUS "/opt/digit/corpus/corpus.tsv"
#define OBS "/opt/digit/observation/observations.jsonl"
#define LESSONS "/opt/digit/lessons"
#define REPORT "digit.irc.deliver"
#define SLACK_REPORT "digit.slack.modules.deliver"
typedef struct {char report[384];} irc_report_t;
typedef struct {char path[1024];time_t mtime;long nsec;off_t size;int seen;} file_state_t;
static pthread_t thread;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int live,started;
static const stnlabz_module_host_t *api;
static file_state_t sources[2]={
 {CORPUS,0,0,0,0},{OBS,0,0,0,0}
};
static int running(void){int v;pthread_mutex_lock(&lock);v=live;pthread_mutex_unlock(&lock);return v;}
/* Independent bounded queues: IRC downtime cannot suppress Slack reporting. */
#define REPORT_CAP 64
static irc_report_t irc_pending[REPORT_CAP],slack_pending[REPORT_CAP];
static unsigned irc_count,slack_count;
static void flush_one(const char *service,irc_report_t *queue,unsigned *count){
 while(*count&&api&&api->invoke_service){
  size_t used=0;
  if(api->invoke_service(service,&queue[0],sizeof queue[0],
      NULL,0,&used)!=STNLABZ_MODULE_OK)break;
  --*count;
  if(*count)memmove(queue,queue+1,(*count)*sizeof queue[0]);
 }
}
static void flush_reports(void){
 flush_one(REPORT,irc_pending,&irc_count);
 flush_one(SLACK_REPORT,slack_pending,&slack_count);
}
static void push_report(irc_report_t *queue,unsigned *count,const irc_report_t *item,const char *name){
 if(*count==REPORT_CAP){
  /* Bound memory and preserve newer operational information. */
  memmove(queue,queue+1,(REPORT_CAP-1)*sizeof queue[0]);
  --*count;
  syslog(LOG_WARNING,"digit.sqlite3 ingest: %s report queue overflow, oldest dropped",name);
 }
 queue[(*count)++]=*item;
}
static void report(const char *name,unsigned add,unsigned change,unsigned skip,int error){
 irc_report_t irc={{0}},slack={{0}};
 snprintf(irc.report,sizeof irc.report,
   "[SQLite sync] %.100s: added=%u updated=%u skipped=%u %s",
   name,add,change,skip,error?"FAILED":"OK");
 snprintf(slack.report,sizeof slack.report,
   "[SQLITE MEMORY] %.100s: added=%u updated=%u skipped=%u %s",
   name,add,change,skip,error?"FAILED":"OK");
 syslog(error?LOG_ERR:LOG_INFO,"%s",slack.report);
 push_report(irc_pending,&irc_count,&irc,"IRC");
 push_report(slack_pending,&slack_count,&slack,"Slack");
 flush_reports();
}
static int begin(sqlite3 *db){return sqlite3_exec(db,"BEGIN IMMEDIATE",NULL,NULL,NULL)==SQLITE_OK;}
static int commit(sqlite3 *db){return sqlite3_exec(db,"COMMIT",NULL,NULL,NULL)==SQLITE_OK;}
static int corpus_upsert(sqlite3 *db,const char *id,const char *category,
 const char *origin,const char *body,unsigned *add,unsigned *change,unsigned *skip){
 sqlite3_stmt *st=NULL;int rc,exists=0;const unsigned char *old;const unsigned char *status;
 if(!*id||!*body)return 0;
 rc=sqlite3_prepare_v2(db,"SELECT text,verification_status FROM corpus WHERE source_record_id=?1 LIMIT 1",-1,&st,NULL);
 if(rc!=SQLITE_OK)return 0;
 sqlite3_bind_text(st,1,id,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(st);
 if(rc==SQLITE_ROW){
  exists=1;old=sqlite3_column_text(st,0);
  status=sqlite3_column_text(st,1);
  if(old&&!strcmp((const char*)old,body)&&status&&
     !strcmp((const char*)status,"operator-verified")){++*skip;sqlite3_finalize(st);return 1;}
 }else if(rc!=SQLITE_DONE){sqlite3_finalize(st);return 0;}
 sqlite3_finalize(st);st=NULL;
 /* Adopt an identical preexisting row instead of adding a duplicate.
  * A matching designated source is operator-approved even when an older
  * importer left the row without provenance or as unverified. */
 if(!exists){
  rc=sqlite3_prepare_v2(db,
   "SELECT id FROM corpus WHERE category=?1 AND text=?2 LIMIT 1",-1,&st,NULL);
  if(rc!=SQLITE_OK)return 0;
  sqlite3_bind_text(st,1,category,-1,SQLITE_TRANSIENT);
  sqlite3_bind_text(st,2,body,-1,SQLITE_TRANSIENT);
  rc=sqlite3_step(st);
  if(rc==SQLITE_ROW){
   sqlite3_int64 rowid=sqlite3_column_int64(st,0);
   sqlite3_finalize(st);st=NULL;
   rc=sqlite3_prepare_v2(db,
    "UPDATE corpus SET verification_status='operator-verified',"
    "source_record_id=CASE WHEN source_record_id IS NULL OR source_record_id='' "
    "THEN ?1 ELSE source_record_id END WHERE id=?2",-1,&st,NULL);
   if(rc!=SQLITE_OK)return 0;
   sqlite3_bind_text(st,1,id,-1,SQLITE_TRANSIENT);
   sqlite3_bind_int64(st,2,rowid);
   rc=sqlite3_step(st);sqlite3_finalize(st);
   if(rc==SQLITE_DONE)++*change;
   return rc==SQLITE_DONE;
  }
  sqlite3_finalize(st);st=NULL;
  if(rc!=SQLITE_DONE)return 0;
 }
 rc=sqlite3_prepare_v2(db,exists?
   "UPDATE corpus SET category=?1,source=?2,text=?3,verification_status='operator-verified' WHERE source_record_id=?4":
   "INSERT INTO corpus(category,source,text,verification_status,source_record_id) "
   "SELECT ?1,?2,?3,'operator-verified',?4 WHERE NOT EXISTS "
   "(SELECT 1 FROM corpus WHERE category=?1 AND text=?3)",-1,&st,NULL);
 if(rc!=SQLITE_OK)return 0;
 sqlite3_bind_text(st,1,category,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(st,2,origin,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(st,3,body,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(st,4,id,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(st);
 if(rc==SQLITE_DONE){
  if(sqlite3_changes(db)){if(exists)++*change;else ++*add;}
  else ++*skip;
 }
 sqlite3_finalize(st);return rc==SQLITE_DONE;
}
static int observation_upsert(sqlite3 *db,const char *line,unsigned *add,unsigned *skip){
 sqlite3_stmt *st=NULL;int rc;char hashid[64];uint64_t hash=UINT64_C(14695981039346656037);
 const unsigned char *p=(const unsigned char*)line;
 while(*p){hash^=*p++;hash*=UINT64_C(1099511628211);}
 snprintf(hashid,sizeof hashid,"import:jsonl:%016" PRIx64,hash);
 /* Required: valid JSON object with nonempty text or message.
  * Source identifiers are honored for dedup, never fabricated timestamps
  * or authenticated speaker identities. */
 rc=sqlite3_prepare_v2(db,
  "INSERT INTO observations(event_id,conversation_id,text,source_record_id,timestamp) "
  "SELECT ?1,c.id,"
  "COALESCE(json_extract(?2,'$.text'),json_extract(?2,'$.message')),"
  "COALESCE(json_extract(?2,'$.event_id'),json_extract(?2,'$.source_record_id'),?1),"
  "COALESCE(json_extract(?2,'$.timestamp'),datetime('now')) "
  "FROM conversations c WHERE c.transport='irc' AND c.channel='#stn-labz' "
  "AND json_valid(?2) "
  "AND length(COALESCE(json_extract(?2,'$.text'),json_extract(?2,'$.message')))>0 "
  "AND NOT EXISTS(SELECT 1 FROM observations WHERE event_id=?1 OR "
  "source_record_id=COALESCE(json_extract(?2,'$.event_id'),"
  "json_extract(?2,'$.source_record_id'),?1))",-1,&st,NULL);
 if(rc!=SQLITE_OK)return 0;
 sqlite3_bind_text(st,1,hashid,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(st,2,line,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(st);sqlite3_finalize(st);
 if(rc!=SQLITE_DONE)return 0;
 if(!sqlite3_changes(db)){++*skip;return 1;}
 rc=sqlite3_prepare_v2(db,
  "INSERT INTO context(subject_type,subject_id,context_type,value,source_id,confidence) "
  "SELECT 'conversation',conversation_id,'utterance.observed',text,event_id,1.0 "
  "FROM observations WHERE event_id=?1",-1,&st,NULL);
 if(rc!=SQLITE_OK)return 0;
 sqlite3_bind_text(st,1,hashid,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(st);sqlite3_finalize(st);
 if(rc!=SQLITE_DONE)return 0;
 ++*add;return 1;
}
static int ingest(sqlite3 *db,const char *path,int type){
 FILE *fp=fopen(path,"rb");char *line=NULL;size_t cap=0;ssize_t n;
 if(!fp){syslog(LOG_ERR,"digit.sqlite3 ingest: cannot open %s",path);report(path,0,0,0,1);return 0;}
 unsigned add=0,change=0,skip=0,batch=0;int transaction_open=0,ok=1;unsigned long line_number=0;const char *reason="unknown";const char *name=strrchr(path,'/');
 if(!begin(db)){
  syslog(LOG_ERR,"digit.sqlite3 ingest: transaction begin failed %s: %s",path,sqlite3_errmsg(db));
  {char detail[180];int code=sqlite3_extended_errcode(db);
   snprintf(detail,sizeof detail,"%s begin transaction (SQLite %d)",name?name+1:path,code);
   report(detail,0,0,0,1);
  }
  fclose(fp);return 0;
 }
 transaction_open=1;
 if(type==2){
  long len;char id[256];char *body;
  if(fseek(fp,0,SEEK_END)||(len=ftell(fp))<0||len>1048576||
     fseek(fp,0,SEEK_SET)){ok=0;goto done;}
  body=malloc((size_t)len+1);
  if(!body){ok=0;goto done;}
  if(fread(body,1,(size_t)len,fp)!=(size_t)len){free(body);ok=0;goto done;}
  body[len]=0;snprintf(id,sizeof id,"lesson:%s",name?name+1:path);
  if(len&&!corpus_upsert(db,id,"LESSON",path,body,&add,&change,&skip))ok=0;
  free(body);goto done;
 }
 /* Finish an already-started transaction before shutdown. A hotload
  * may request stop while scanning a large source; that is not a read
  * error and must not discard thousands of valid imported rows. */
 while((n=getline(&line,&cap,fp))!=-1){
  line_number++;
  if(!(line_number%5000))syslog(LOG_INFO,"digit.sqlite3 ingest: scanning %s line=%lu",path,line_number);
  if(n&&line[n-1]=='\n')line[--n]=0;
  if(n&&line[n-1]=='\r')line[--n]=0;
  if(!n)continue;
  if(type==0){
   char *a=strchr(line,'\t'),*b,*c;
   if(!a||(b=strchr(a+1,'\t'))==NULL||(c=strchr(b+1,'\t'))==NULL){reason="invalid TSV fields";ok=0;break;}
   *a++=0;*b++=0;*c++=0;
   if(!corpus_upsert(db,line,b,a,c,&add,&change,&skip)){reason="Corpus SQL write";ok=0;break;}
  }else if(!observation_upsert(db,line,&add,&skip)){reason="Observation JSON/SQL write";ok=0;break;}
  /* Release the writer lock frequently: IRC observations must not be
   * starved by a full-file import. Previously 48K rows were one transaction. */
  if(++batch==100){
   if(!commit(db)){reason="batch commit";ok=0;break;}
   transaction_open=0;batch=0;
   if(!running())break;
   {struct timespec pause={0,20000000};nanosleep(&pause,NULL);}
   if(!begin(db)){reason="batch begin";ok=0;break;}
   transaction_open=1;
  }
 }

 if(ferror(fp)){reason="file read error";ok=0;}
 if(ok&&!running()&&!feof(fp)){
  reason="paused for module shutdown";
 }
done:
 free(line);fclose(fp);
 if(ok&&transaction_open&&!commit(db)){reason="commit";ok=0;}
 if(!ok){
  char error_report[170];
  snprintf(error_report,sizeof error_report,"%s line %lu %s (SQLite %d)",
    name?name+1:path,line_number,reason,sqlite3_extended_errcode(db));
  report(error_report,0,0,skip,1);
  syslog(LOG_ERR,"digit.sqlite3 ingest: rollback %s line=%lu stage=%s sqlite=%d %s",
    path,line_number,reason,sqlite3_extended_errcode(db),sqlite3_errmsg(db));
  if(transaction_open)(void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
 }
 if(ok)report(name?name+1:path,add,change,skip,0);
 /* Do not mark a partially processed file as fully synchronized. */
 return ok&&strcmp(reason,"paused for module shutdown")!=0;
}
static void *watcher(void *unused){
 sqlite3 *db=NULL;(void)unused;
 if(sqlite3_open_v2(DB,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK){
  int code=db?sqlite3_extended_errcode(db):SQLITE_CANTOPEN;
  syslog(LOG_ERR,"digit.sqlite3 ingest: open failed SQLite %d: %s",
      code,db?sqlite3_errmsg(db):"no database handle");
  if(db)sqlite3_close(db);
  report("database open",0,0,0,1);return NULL;
 }
 sqlite3_busy_timeout(db,5000);
 /* Without indexes, each of ~49K Corpus lines scans the entire table
  * twice. That is quadratic and makes the initial transaction appear hung. */
 if(sqlite3_exec(db,
    "CREATE INDEX IF NOT EXISTS idx_digit_ingest_corpus_source ON corpus(source_record_id);"
    "CREATE INDEX IF NOT EXISTS idx_digit_ingest_corpus_content ON corpus(category,text);"
    "CREATE INDEX IF NOT EXISTS idx_digit_ingest_observation_source ON observations(source_record_id);",
    NULL,NULL,NULL)!=SQLITE_OK){
   syslog(LOG_ERR,"digit.sqlite3 ingest: index setup failure: %s",sqlite3_errmsg(db));
   report("ingest indexes",0,0,0,1);
   sqlite3_close(db);return NULL;
 }
 report("watcher started: corpus.tsv, observations.jsonl, lessons/*.txt",0,0,0,0);
 while(running()){
  int i;DIR *dir;
  for(i=0;i<2&&running();i++){
   struct stat st;file_state_t *f=&sources[i];
   if(stat(f->path,&st)||!S_ISREG(st.st_mode))continue;
   if(!f->seen||st.st_size!=f->size||st.st_mtim.tv_sec!=f->mtime||
      st.st_mtim.tv_nsec!=f->nsec){
    if(ingest(db,f->path,i)){
     f->seen=1;f->size=st.st_size;f->mtime=st.st_mtim.tv_sec;f->nsec=st.st_mtim.tv_nsec;
    }
   }
  }
  dir=opendir(LESSONS);
  if(dir){
   struct dirent *e;
   while(running()&&(e=readdir(dir))!=NULL){
    char path[1024];struct stat st;size_t len=strlen(e->d_name);
    if(len<5||strcmp(e->d_name+len-4,".txt"))continue;
    if(snprintf(path,sizeof path,"%s/%s",LESSONS,e->d_name)>=(int)sizeof path||
       lstat(path,&st)||!S_ISREG(st.st_mode))continue;
    {unsigned k;file_state_t *state=NULL;
     for(k=0;k<lesson_count;k++)if(!strcmp(lesson_states[k].path,path)){
       state=&lesson_states[k];break;
     }
     if(!state&&lesson_count<256){
       state=&lesson_states[lesson_count++];
       snprintf(state->path,sizeof state->path,"%s",path);
     }
     if(state&&(!state->seen||state->size!=st.st_size||
        state->mtime!=st.st_mtim.tv_sec||
        state->nsec!=st.st_mtim.tv_nsec)){
       if(ingest(db,path,2)){
        state->seen=1;state->size=st.st_size;
        state->mtime=st.st_mtim.tv_sec;state->nsec=st.st_mtim.tv_nsec;
       }
     }
    }
   }
   closedir(dir);
  }
  flush_reports();
  for(i=0;i<20&&running();i++){
   struct timespec pause={0,100000000};nanosleep(&pause,NULL);
  }
 }
 sqlite3_close(db);return NULL;
}
int digit_ingest_start(const stnlabz_module_host_t *h){
 if(!h||!h->invoke_service)return 0;
 pthread_mutex_lock(&lock);
 if(started){pthread_mutex_unlock(&lock);return 0;}
 api=h;live=1;
 if(pthread_create(&thread,NULL,watcher,NULL)!=0){
  live=0;api=NULL;pthread_mutex_unlock(&lock);return 0;
 }
 started=1;pthread_mutex_unlock(&lock);return 1;
}
void digit_ingest_stop(void){
 pthread_mutex_lock(&lock);
 if(!started){pthread_mutex_unlock(&lock);return;}
 live=0;pthread_mutex_unlock(&lock);
 pthread_join(thread,NULL);
 pthread_mutex_lock(&lock);started=0;api=NULL;pthread_mutex_unlock(&lock);
}
