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
static irc_report_t pending[64];
static unsigned pending_count;
static file_state_t lesson_states[256];
static unsigned lesson_count;
static void flush_reports(void){
 while(pending_count&&api&&api->invoke_service){
  size_t used=0;
  if(api->invoke_service(REPORT,&pending[0],sizeof pending[0],
      NULL,0,&used)!=STNLABZ_MODULE_OK)break;
  memmove(pending,pending+1,(--pending_count)*sizeof pending[0]);
 }
}
static void report(const char *name,unsigned add,unsigned change,unsigned skip,int error){
 irc_report_t r={{0}};
 snprintf(r.report,sizeof r.report,"[SQLite sync] %.100s: added=%u updated=%u skipped=%u %s",
 name,add,change,skip,error?"FAILED":"OK");
 syslog(error?LOG_ERR:LOG_INFO,"%s",r.report);
 if(pending_count==sizeof pending/sizeof pending[0]){
  syslog(LOG_ERR,"digit.sqlite3 ingest: IRC report queue full");
  return;
 }
 pending[pending_count++]=r;
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
 unsigned add=0,change=0,skip=0;int ok=1;const char *name=strrchr(path,'/');
 if(!begin(db)){
  syslog(LOG_ERR,"digit.sqlite3 ingest: transaction begin failed %s: %s",path,sqlite3_errmsg(db));
  fclose(fp);report(path,0,0,0,1);return 0;
 }
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
 while(running()&&(n=getline(&line,&cap,fp))!=-1){
  if(n&&line[n-1]=='\n')line[--n]=0;
  if(n&&line[n-1]=='\r')line[--n]=0;
  if(!n)continue;
  if(type==0){
   char *a=strchr(line,'\t'),*b,*c;
   if(!a||(b=strchr(a+1,'\t'))==NULL||(c=strchr(b+1,'\t'))==NULL){ok=0;break;}
   *a++=0;*b++=0;*c++=0;
   if(!corpus_upsert(db,line,b,a,c,&add,&change,&skip)){ok=0;break;}
  }else if(!observation_upsert(db,line,&add,&skip)){ok=0;break;}
 }
 if(ferror(fp)||!running())ok=0;
done:
 free(line);fclose(fp);
 if(ok)ok=commit(db);
 if(!ok){syslog(LOG_ERR,"digit.sqlite3 ingest: import rollback %s: %s",path,sqlite3_errmsg(db));
  (void)sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL);
 }
 report(name?name+1:path,ok?add:0,ok?change:0,skip,!ok);
 return ok;
}
static void *watcher(void *unused){
 sqlite3 *db=NULL;(void)unused;
 if(sqlite3_open_v2(DB,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL)!=SQLITE_OK){
  if(db)sqlite3_close(db);syslog(LOG_ERR,"digit.sqlite3 ingest: database unavailable");return NULL;
 }
 sqlite3_busy_timeout(db,5000);
 report("watcher started: corpus.tsv, observations.jsonl, lessons/*.txt",0,0,0,0);
 while(running()){
  int i;DIR *dir;
  for(i=0;i<2;i++){
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
