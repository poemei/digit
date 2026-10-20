/* [AI:GPT-6 | 2026-10-18] Digit SQLite3 persistence module, ABI 1.5.
 * The database is owned by this module; Core owns lifecycle and authorization.
 */
#define _POSIX_C_SOURCE 200809L
#include <sqlite3.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <syslog.h>
#include "module.h"
#include "sqlite3_diagnostic.h"
#include "ingest.h"
#include "corpus_search.h"
#include "context.h"
#include "../observation/observation.h"
#include "../s2/s2_observations.h"
#include "dsd.h"

#define DB_PATH "/opt/digit/data/digit_memory.db"
#define KEY_MAX 128
#define VALUE_MAX 1024

typedef struct { char key[KEY_MAX]; unsigned char value[VALUE_MAX]; size_t length; } sqlite3_record_t;
static sqlite3 *database;
static sqlite3 *observation_writer;
static const stnlabz_module_host_t *host;

static void observe_search(unsigned count,int failed,unsigned line){
 uint64_t trace=0,query_token=1;size_t used=0;digit_dsd_record_t rec={0};
 if(!host||!host->invoke_service)return;
 if(host->invoke_service(DIGIT_DSD_CURRENT_SERVICE,&query_token,sizeof query_token,&trace,
    sizeof trace,&used)!=STNLABZ_MODULE_OK||used!=sizeof trace||!trace)return;
 rec.trace_id=rec.event.trace_id=trace;
 rec.event.span_id=2;rec.event.parent_span_id=1;
 rec.event.line=line;rec.event.result=failed?1:0;rec.event.records=count;
 snprintf(rec.event.module,sizeof rec.event.module,"sqlite3");
 snprintf(rec.event.method,sizeof rec.event.method,"corpus_search");
 snprintf(rec.event.file,sizeof rec.event.file,"modules/sqlite3/sqlite3.c");
 snprintf(rec.event.operation,sizeof rec.event.operation,"corpus_fts");
 snprintf(rec.event.status,sizeof rec.event.status,"%s",
          failed?"QUERY_ERROR":count?"QUERY_MATCH":"QUERY_NO_MATCH");
 (void)host->invoke_service(DIGIT_DSD_RECORD_SERVICE,&rec,sizeof rec,
                            NULL,0,&used);
}
/* Report actual context-query outcomes, only for a bound DSD request.
 * No row values, SQL text, or source accounts enter diagnostics. */
static void observe_context(unsigned count,int sqlite_code,unsigned line){
 uint64_t trace=0,token=1;size_t used=0;digit_dsd_record_t rec={0};
 if(!host||!host->invoke_service)return;
 if(host->invoke_service(DIGIT_DSD_CURRENT_SERVICE,&token,sizeof token,
    &trace,sizeof trace,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof trace||!trace)return;
 rec.trace_id=rec.event.trace_id=trace;
 rec.event.span_id=4;rec.event.parent_span_id=3;
 rec.event.line=line;rec.event.result=sqlite_code==SQLITE_OK?0:1;
 rec.event.records=count;
 snprintf(rec.event.module,sizeof rec.event.module,"sqlite3");
 snprintf(rec.event.method,sizeof rec.event.method,"context_recent");
 snprintf(rec.event.file,sizeof rec.event.file,"modules/sqlite3/sqlite3.c");
 snprintf(rec.event.operation,sizeof rec.event.operation,"context_select");
 snprintf(rec.event.status,sizeof rec.event.status,"%s",
   sqlite_code==SQLITE_OK?"SQLITE_OK":
   sqlite_code==SQLITE_BUSY?"SQLITE_BUSY":
   sqlite_code==SQLITE_LOCKED?"SQLITE_LOCKED":
   sqlite_code==SQLITE_READONLY?"SQLITE_READONLY":
   sqlite_code==SQLITE_CORRUPT?"SQLITE_CORRUPT":
   sqlite_code==SQLITE_IOERR?"SQLITE_IOERR":"QUERY_ERROR");
 (void)host->invoke_service(DIGIT_DSD_RECORD_SERVICE,&rec,sizeof rec,NULL,0,&used);
}
static int key_valid(const char *key) {
    size_t i;
    if (!key || !key[0]) return 0;
    for (i=0;i<KEY_MAX;i++) {
        unsigned char ch=(unsigned char)key[i];
        if (!ch) return i>0;
        if (!(ch>='a'&&ch<='z') && !(ch>='A'&&ch<='Z') &&
            !(ch>='0'&&ch<='9') && ch!='.' && ch!='_' && ch!='-' && ch!='/')
            return 0;
    }
    return 0;
}
static int get(sqlite3 *db,sqlite3_record_t *r) {
    sqlite3_stmt *stmt=NULL;int ok=0;
    if (!db || !r || !key_valid(r->key)) return 0;
    if(sqlite3_prepare_v2(db,
        "SELECT value FROM digit_kv WHERE key=?1",-1,&stmt,NULL)!=SQLITE_OK) return 0;
    if(sqlite3_bind_text(stmt,1,r->key,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
       sqlite3_step(stmt)==SQLITE_ROW) {
        int bytes=sqlite3_column_bytes(stmt,0);
        if(bytes>=0 && bytes<=VALUE_MAX) {
            const void *data=sqlite3_column_blob(stmt,0);
            if(bytes && data) memcpy(r->value,data,(size_t)bytes);
            r->length=(size_t)bytes;ok=1;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}
/* Literal FTS5 phrase from request text. Bind as parameter; never embed
 * user input in SQL. The verified schema supplies corpus_fts and corpus. */
static int fts_phrase(const char *input,char *out,size_t cap){
 size_t i=0,j=0;int in_word=0,seen=0;
 if(!input||!out||cap<4)return 0;
 out[j++]='"';
 while(input[i]){
  unsigned char c=(unsigned char)input[i++];
  if(isalnum(c)||c=='_'){
   if(j+3>=cap)return 0;
   out[j++]=(char)c;in_word=1;seen=1;
  }else if(in_word){
   if(j+3>=cap)return 0;
   out[j++]=' ';in_word=0;
  }
 }
 if(!seen)return 0;
 if(j>1&&out[j-1]==' ')j--;
 out[j++]='"';out[j]=0;return 1;
}
static void copy_column(sqlite3_stmt *stmt,int index,char *dest,size_t cap){
 const unsigned char *src=sqlite3_column_text(stmt,index);
 size_t i=0;
 if(!cap)return;
 if(src)while(src[i]&&i+1<cap){
  unsigned char c=src[i];dest[i]=(c<32||c==127)?' ':(char)c;i++;
 }
 dest[i]=0;
}
static int corpus_search(sqlite3 *db,const digit_corpus_query_t *query,
                         digit_corpus_result_t *out){
 static const char sql[]=
  "SELECT c.id,c.category,c.verification_status,substr(c.text,1,900) "
  "FROM corpus_fts JOIN corpus c ON c.id=corpus_fts.rowid "
  "WHERE corpus_fts MATCH ?1 ORDER BY bm25(corpus_fts) LIMIT 3";
 sqlite3_stmt *stmt=NULL;
 char phrase[2*DIGIT_CORPUS_QUERY_MAX+4];int rc;
 if(!db||!query||!out||!memchr(query->query,0,sizeof query->query)||
    !fts_phrase(query->query,phrase,sizeof phrase))return -1;
 memset(out,0,sizeof *out);
 rc=sqlite3_prepare_v2(db,sql,-1,&stmt,NULL);
 if(rc!=SQLITE_OK)return -1;
 if(sqlite3_bind_text(stmt,1,phrase,-1,SQLITE_TRANSIENT)!=SQLITE_OK){
  sqlite3_finalize(stmt);return -1;
 }
 while((rc=sqlite3_step(stmt))==SQLITE_ROW){
  digit_corpus_hit_t *hit;
  if(out->count>=DIGIT_CORPUS_RESULTS_MAX)break;
  hit=&out->hits[out->count++];
  hit->id=sqlite3_column_int64(stmt,0);
  copy_column(stmt,1,hit->category,sizeof hit->category);
  /* Preserve the database's verification status without modification. */
  copy_column(stmt,2,hit->verification_status,sizeof hit->verification_status);
  copy_column(stmt,3,hit->excerpt,sizeof hit->excerpt);
 }
 sqlite3_finalize(stmt);
 return (rc==SQLITE_DONE||rc==SQLITE_ROW)?(int)out->count:-1;
}
/* Observation is the only permitted write operation. Keep the main corpus
 * connection READONLY. The writer must never execute caller-provided SQL. */
/* Observation and context are one SQLite transaction. An accepted
 * observation cannot become visible without its provenance-linked context.
 * The module owns all writes; Core and the Observation ABI remain unchanged. */
static int sqlite_qualification_active;
static stnlabz_module_result_t observation_insert(
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *response_used,void *context){
 const digit_observation_record_t *r=request;
 sqlite3_stmt *stmt=NULL;
 sqlite3_int64 person=0,conversation=0,observation_id=0;
 digit_observation_receipt_t receipt={0};
 int wants_receipt=response!=NULL;
 int rc;
 const char *phase="validate";
 (void)context;
 if(response_used)*response_used=0;
 if((wants_receipt && (!response_used || response_size<sizeof receipt)) ||
    !observation_writer||!r||request_size!=sizeof *r||
    !memchr(r->sender,0,sizeof r->sender)||
    !memchr(r->channel,0,sizeof r->channel)||
    !memchr(r->transport,0,sizeof r->transport)||
    !memchr(r->account,0,sizeof r->account)||
    !memchr(r->text,0,sizeof r->text)||
    !r->sender[0]||!r->text[0]||
    strcmp(r->transport,"irc")||strcmp(r->channel,"#stn-labz"))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 phase="lookup_person";
 rc=sqlite3_prepare_v2(observation_writer,
   "SELECT id FROM people WHERE display_name=?1 LIMIT 1",-1,&stmt,NULL);
 if(rc!=SQLITE_OK){
   syslog(LOG_ERR,"digit.sqlite3 observation_atomic status=rejected phase=%s sqlite_code=%d",phase,rc);
   return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 sqlite3_bind_text(stmt,1,r->sender,-1,SQLITE_TRANSIENT);
 if(sqlite3_step(stmt)==SQLITE_ROW)person=sqlite3_column_int64(stmt,0);
 sqlite3_finalize(stmt);stmt=NULL;
 phase="lookup_conversation";
 rc=sqlite3_prepare_v2(observation_writer,
   "SELECT id FROM conversations WHERE transport=?1 AND channel=?2 LIMIT 1",
   -1,&stmt,NULL);
 if(rc!=SQLITE_OK)return STNLABZ_MODULE_ERR_INVALID_STATE;
 sqlite3_bind_text(stmt,1,r->transport,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,2,r->channel,-1,SQLITE_TRANSIENT);
 if(sqlite3_step(stmt)==SQLITE_ROW)conversation=sqlite3_column_int64(stmt,0);
 sqlite3_finalize(stmt);stmt=NULL;
 if(!conversation){
   syslog(LOG_ERR,"digit.sqlite3 observation_atomic status=rejected phase=conversation_missing");
   return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 phase="begin_transaction";
 if(sqlite3_exec(observation_writer,"BEGIN IMMEDIATE",NULL,NULL,NULL)!=SQLITE_OK){
     syslog(LOG_ERR,"digit.sqlite3 observation_atomic status=rejected phase=begin_transaction sqlite_code=%d",sqlite3_errcode(observation_writer));
     return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 phase="insert_observation";
 rc=sqlite3_prepare_v2(observation_writer,
   "INSERT INTO observations(event_id,sender_id,conversation_id,text,timestamp,source_record_id) "
   "VALUES ('irc-live-'||lower(hex(randomblob(16))),?1,?2,?3,"
   "strftime('%Y-%m-%d %H:%M:%S','now'),?4)",-1,&stmt,NULL);
 if(rc!=SQLITE_OK)goto rollback;
 if(person)sqlite3_bind_int64(stmt,1,person);
 else sqlite3_bind_null(stmt,1);
 sqlite3_bind_int64(stmt,2,conversation);
 sqlite3_bind_text(stmt,3,r->text,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,4,r->account,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(stmt);
 sqlite3_finalize(stmt);stmt=NULL;
 if(rc!=SQLITE_DONE)goto rollback;
 observation_id=sqlite3_last_insert_rowid(observation_writer);
 phase="insert_context";
 rc=sqlite3_prepare_v2(observation_writer,
   "INSERT INTO context(subject_type,subject_id,context_type,value,source_id,confidence,expires_at) "
   "SELECT 'conversation',?1,'utterance.observed',text,event_id,1.0,NULL "
   "FROM observations WHERE id=?2 AND conversation_id=?1",
   -1,&stmt,NULL);
 if(rc!=SQLITE_OK)goto rollback;
 sqlite3_bind_int64(stmt,1,conversation);
 sqlite3_bind_int64(stmt,2,observation_id);
 rc=sqlite3_step(stmt);
 sqlite3_finalize(stmt);stmt=NULL;
 if(rc!=SQLITE_DONE||sqlite3_changes(observation_writer)!=1)goto rollback;
 /* Verify exact provenance on the writer before committing. */
 phase="verify_pair";
 rc=sqlite3_prepare_v2(observation_writer,
   "SELECT o.event_id,x.id FROM observations o "
   "JOIN context x ON x.source_id=o.event_id "
   "WHERE o.id=?1 AND x.subject_type='conversation' "
   "AND x.subject_id=?2 AND x.context_type='utterance.observed' "
   "AND x.value=o.text LIMIT 1",-1,&stmt,NULL);
 if(rc!=SQLITE_OK)goto rollback;
 sqlite3_bind_int64(stmt,1,observation_id);
 sqlite3_bind_int64(stmt,2,conversation);
 rc=sqlite3_step(stmt);
 if(rc!=SQLITE_ROW)goto rollback;
 receipt.observation_id=observation_id;
 receipt.context_id=sqlite3_column_int64(stmt,1);
 snprintf(receipt.event_id,sizeof receipt.event_id,"%s",
          (const char *)sqlite3_column_text(stmt,0));
 sqlite3_finalize(stmt);stmt=NULL;
 if(receipt.context_id<=0||!receipt.event_id[0])goto rollback;
 phase="commit";
 if(sqlite3_exec(observation_writer,"COMMIT",NULL,NULL,NULL)!=SQLITE_OK)
     goto rollback;
 /* An explicit receipt is returned only after COMMIT succeeds. */
 if(wants_receipt){
   memcpy(response,&receipt,sizeof receipt);
   *response_used=sizeof receipt;
 }
 if(!sqlite_qualification_active)
   syslog(LOG_INFO,
     "digit.sqlite3 observation_atomic role=runtime status=committed observation_id=%lld context_id=%lld",
     (long long)receipt.observation_id,(long long)receipt.context_id);
 return STNLABZ_MODULE_OK;
rollback:
 if(stmt)sqlite3_finalize(stmt);
 syslog(LOG_ERR,"digit.sqlite3 observation_atomic status=rollback phase=%s sqlite_code=%d",phase,sqlite3_errcode(observation_writer));
 (void)sqlite3_exec(observation_writer,"ROLLBACK",NULL,NULL,NULL);
 return STNLABZ_MODULE_ERR_INVALID_STATE;
}
/* Bounded, read-only IRC context; exact channel match, oldest first.
 * SQL text is fixed and caller data is only ever bound as a parameter. */
static stnlabz_module_result_t observation_recent(
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *response_used,void *context){
 const digit_observation_recent_query_t *q=request;
 digit_observation_recent_result_t out={0};
 sqlite3_stmt *stmt=NULL;
 int rc;unsigned i=0;
 (void)context;
 if(response_used)*response_used=0;
 if(!database||!q||request_size!=sizeof *q||!response||
    response_size<sizeof out||!response_used||
    !memchr(q->channel,0,sizeof q->channel)||
    strcmp(q->channel,"#stn-labz"))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 rc=sqlite3_prepare_v2(database,
  "SELECT COALESCE(p.display_name,''),o.text FROM ("
  "SELECT o.sender_id,o.text,o.id FROM observations o "
  "JOIN conversations c ON c.id=o.conversation_id "
  "WHERE c.transport='irc' AND c.channel=?1 "
  "ORDER BY o.id DESC LIMIT 6) o "
  "LEFT JOIN people p ON p.id=o.sender_id ORDER BY o.id ASC",
  -1,&stmt,NULL);
 if(rc!=SQLITE_OK)return STNLABZ_MODULE_ERR_INVALID_STATE;
 sqlite3_bind_text(stmt,1,q->channel,-1,SQLITE_TRANSIENT);
 while((rc=sqlite3_step(stmt))==SQLITE_ROW&&i<DIGIT_OBSERVATION_RECENT_MAX){
  copy_column(stmt,0,out.items[i].sender,sizeof out.items[i].sender);
  copy_column(stmt,1,out.items[i].text,sizeof out.items[i].text);
  i++;
 }
 sqlite3_finalize(stmt);
 if(rc!=SQLITE_DONE&&rc!=SQLITE_ROW)return STNLABZ_MODULE_ERR_INVALID_STATE;
 out.count=i;
 memcpy(response,&out,sizeof out);*response_used=sizeof out;
 return STNLABZ_MODULE_OK;
}

/* Context is derived only from a persisted IRC observation. The referenced
 * event_id, conversation ID and original message must exist; never invent
 * provenance or treat a lexical category as factual knowledge. */
static stnlabz_module_result_t context_store(
 const void *request,size_t size,void *response,size_t capacity,
 size_t *used,void *context){
 const digit_context_store_request_t *r=request;
 sqlite3_stmt *stmt=NULL;
 const char *category;
 int rc;
 (void)response;(void)capacity;(void)context;
 if(used)*used=0;
 if(!observation_writer||!r||size!=sizeof *r||
    !memchr(r->channel,0,sizeof r->channel)||
    !memchr(r->sender,0,sizeof r->sender)||
    !memchr(r->source_account,0,sizeof r->source_account)||
    !memchr(r->text,0,sizeof r->text)||
    !r->sender[0]||!r->text[0]||!r->source_account[0]||
    strcmp(r->channel,"#stn-labz")||
    (r->kind!=1&&r->kind!=2))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 category=r->kind==2?"utterance.question":"utterance.statement";
 /* A single bounded context fact per observation source. */
 rc=sqlite3_prepare_v2(observation_writer,
  "INSERT INTO context(subject_type,subject_id,context_type,value,source_id,confidence,expires_at) "
  "SELECT 'conversation',latest.conversation_id,?1,latest.text,latest.event_id,1.0,NULL "
  "FROM (SELECT o.conversation_id,o.text,o.event_id FROM observations o "
  "JOIN conversations c ON c.id=o.conversation_id "
  "WHERE c.transport='irc' AND c.channel=?2 "
  "AND (o.text=?3 OR (length(o.text)>length(?3) "
  "AND length(o.text)-length(?3)<=72 "
  "AND substr(o.text,-length(?3))=?3 "
  "AND substr(o.text,length(o.text)-length(?3),1) IN (' ',':',','))) "
  "AND o.source_record_id=?4 ORDER BY o.id DESC LIMIT 1) latest "
  "WHERE NOT EXISTS(SELECT 1 FROM context x WHERE x.subject_type='conversation' "
  "AND x.subject_id=latest.conversation_id AND x.source_id=latest.event_id "
  "AND x.context_type=?1)",
  -1,&stmt,NULL);
 if(rc!=SQLITE_OK)return STNLABZ_MODULE_ERR_INVALID_STATE;
 sqlite3_bind_text(stmt,1,category,-1,SQLITE_STATIC);
 sqlite3_bind_text(stmt,2,r->channel,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,3,r->text,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,4,r->source_account,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(stmt);
 sqlite3_finalize(stmt);
 if(rc!=SQLITE_DONE)return STNLABZ_MODULE_ERR_INVALID_STATE;
 /* Verify a persisted source exists even for idempotent duplicate calls. */
 rc=sqlite3_prepare_v2(observation_writer,
   "SELECT 1 FROM observations o JOIN conversations c "
   "ON c.id=o.conversation_id WHERE c.transport='irc' "
   "AND c.channel=?1 AND (o.text=?2 OR (length(o.text)>length(?2) "
   "AND length(o.text)-length(?2)<=72 "
   "AND substr(o.text,-length(?2))=?2 "
   "AND substr(o.text,length(o.text)-length(?2),1) IN (' ',':',','))) "
   "AND o.source_record_id=?3 "
   "LIMIT 1",-1,&stmt,NULL);
 if(rc!=SQLITE_OK)return STNLABZ_MODULE_ERR_INVALID_STATE;
 sqlite3_bind_text(stmt,1,r->channel,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,2,r->text,-1,SQLITE_TRANSIENT);
 sqlite3_bind_text(stmt,3,r->source_account,-1,SQLITE_TRANSIENT);
 rc=sqlite3_step(stmt);
 sqlite3_finalize(stmt);
 return rc==SQLITE_ROW ? STNLABZ_MODULE_OK :
        rc==SQLITE_DONE ? STNLABZ_MODULE_ERR_NOT_FOUND :
        STNLABZ_MODULE_ERR_INVALID_STATE;
}

static stnlabz_module_result_t context_recent(
 const void *request,size_t size,void *response,size_t capacity,
 size_t *used,void *context){
 const digit_context_recent_query_t *q=request;
 digit_context_recent_result_t out={0};
 sqlite3_stmt *stmt=NULL;
 int rc;
 (void)context;
 if(used)*used=0;
 if(!database||!q||size!=sizeof *q||!response||
    capacity<sizeof out||!used||
    !memchr(q->channel,0,sizeof q->channel)||
    strcmp(q->channel,"#stn-labz"))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 rc=sqlite3_prepare_v2(database,
   "SELECT source_id,context_type,value FROM ("
   "SELECT x.id,x.source_id,x.context_type,x.value "
   "FROM context x JOIN conversations c ON c.id=x.subject_id "
   "WHERE x.subject_type='conversation' AND c.transport='irc' "
   "AND c.channel=?1 AND (x.expires_at IS NULL OR "
   "datetime(x.expires_at)>datetime('now')) "
   "ORDER BY x.id DESC LIMIT 8) ORDER BY id ASC",
   -1,&stmt,NULL);
 if(rc!=SQLITE_OK){observe_context(0,rc,__LINE__);return STNLABZ_MODULE_ERR_INVALID_STATE;}
 sqlite3_bind_text(stmt,1,q->channel,-1,SQLITE_TRANSIENT);
 while((rc=sqlite3_step(stmt))==SQLITE_ROW){
   digit_context_item_t *item;
   if(out.count>=DIGIT_CONTEXT_MAX)break;
   item=&out.items[out.count++];
   copy_column(stmt,0,item->source_id,sizeof item->source_id);
   copy_column(stmt,1,item->context_type,sizeof item->context_type);
   copy_column(stmt,2,item->value,sizeof item->value);
 }
 sqlite3_finalize(stmt);
 if(rc!=SQLITE_DONE&&rc!=SQLITE_ROW)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,&out,sizeof out);
 *used=sizeof out;
 return STNLABZ_MODULE_OK;
}

/* Return newest IRC Rictus Intel observation after an explicit cursor.
 * No rows are modified; callers receive text and provenance ID only. */
static stnlabz_module_result_t s2_observations(
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *response_used,void *context){
 const digit_s2_observations_query_t *q=request;
 digit_s2_observations_result_t out={0};
 sqlite3_stmt *stmt=NULL;
 int rc;
 (void)context;
 if(response_used)*response_used=0;
 if(!database||!q||request_size!=sizeof *q||q->after_id<0||
    !response||response_size<sizeof out||!response_used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 rc=sqlite3_prepare_v2(database,
  "SELECT o.id,o.text FROM observations o "
  "JOIN conversations c ON c.id=o.conversation_id "
  "WHERE o.id>?1 AND c.transport='irc' AND c.channel='#stn-labz' "
  "AND o.text LIKE '[Rictus Intel] %' "
  "ORDER BY o.id ASC LIMIT 1",-1,&stmt,NULL);
 if(rc!=SQLITE_OK)return STNLABZ_MODULE_ERR_INVALID_STATE;
 sqlite3_bind_int64(stmt,1,q->after_id);
 rc=sqlite3_step(stmt);
 if(rc==SQLITE_ROW){
  out.id=sqlite3_column_int64(stmt,0);
  copy_column(stmt,1,out.text,sizeof out.text);
 }
 sqlite3_finalize(stmt);
 if(rc!=SQLITE_ROW&&rc!=SQLITE_DONE)return STNLABZ_MODULE_ERR_INVALID_STATE;
 memcpy(response,&out,sizeof out);*response_used=sizeof out;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t service(const void *request,size_t request_size,
    void *response,size_t response_size,size_t *response_used,void *context) {
    const char *operation=context;
    sqlite3_record_t r;
    if(response_used)*response_used=0;
    if(!database||!operation||!request)return STNLABZ_MODULE_ERR_INVALID_STATE;
    if(!strcmp(operation,"get")) {
        if(request_size!=KEY_MAX||!response||response_size<sizeof r||
           !response_used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        memset(&r,0,sizeof r);
        memcpy(r.key,request,KEY_MAX);
        if(!get(database,&r))return STNLABZ_MODULE_ERR_NOT_FOUND;
        memcpy(response,&r,sizeof r);*response_used=sizeof r;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(operation,"search")){
        digit_corpus_result_t result;int count;
        if(request_size!=sizeof(digit_corpus_query_t)||!response||
           response_size<sizeof result||!response_used)
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        count=corpus_search(database,request,&result);
        observe_search(count<0?0:(unsigned)count,count<0,__LINE__);
        if(count<0)return STNLABZ_MODULE_ERR_INVALID_STATE;
        memcpy(response,&result,sizeof result);
        *response_used=sizeof result;
        return STNLABZ_MODULE_OK;
    }
    return STNLABZ_MODULE_ERR_NOT_FOUND;
}

/* Read-only diagnostic service. This probes SQLite3 at the time requested;
 * it is NOT evidence that a historical user request reached the database. */
static stnlabz_module_result_t diagnostic_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context){
 sqlite3_diagnostic_t out={0};
 sqlite3_stmt *stmt=NULL;
 int code,step_code;
 (void)context;
 if(response_used)*response_used=0;
 if(request||request_size||!response||response_size<sizeof out||!response_used)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 snprintf(out.source,sizeof out.source,"modules/sqlite3/sqlite3.c");
 snprintf(out.method,sizeof out.method,"diagnostic_service");
 out.line=__LINE__+1;
 if(!database){
   snprintf(out.state,sizeof out.state,"UNAVAILABLE");
   snprintf(out.stage,sizeof out.stage,"database_handle");
   snprintf(out.detail,sizeof out.detail,"database handle is not active");
 }else{
   /* Metadata-only prepared read; no user records or values are read. */
   code=sqlite3_prepare_v2(database,
      "SELECT count(*) FROM corpus_fts",
      -1,&stmt,NULL);
   if(code!=SQLITE_OK){
      snprintf(out.state,sizeof out.state,"FAILED");
      snprintf(out.stage,sizeof out.stage,"schema_prepare");
      snprintf(out.detail,sizeof out.detail,"%.80s",sqlite3_errstr(code));
   }else{
      step_code=sqlite3_step(stmt);
      if(step_code==SQLITE_ROW){
       snprintf(out.state,sizeof out.state,"OBSERVED");
       snprintf(out.stage,sizeof out.stage,"corpus_fts");
       snprintf(out.detail,sizeof out.detail,"indexed corpus rows=%lld; data operations not tested",(long long)sqlite3_column_int64(stmt,0));
      }else if(step_code==SQLITE_DONE){
       snprintf(out.state,sizeof out.state,"FAILED");
       snprintf(out.stage,sizeof out.stage,"schema_lookup");
       snprintf(out.detail,sizeof out.detail,"corpus_fts returned no result");
      }else{
       snprintf(out.state,sizeof out.state,"FAILED");
       snprintf(out.stage,sizeof out.stage,"schema_step");
       snprintf(out.detail,sizeof out.detail,"%.80s",sqlite3_errstr(step_code));
      }
   }
 }
 if(stmt)sqlite3_finalize(stmt);
 memcpy(response,&out,sizeof out);*response_used=sizeof out;
 return STNLABZ_MODULE_OK;
}
static const char op_get[]="get",op_search[]="search";
/* [AI:GPT-6 | 2026-10-18] Startup diagnostics go to the service journal.
 * No credentials, SQL row values, or database content are logged.
 */
static void startup_log(const char *stage,int code,const char *detail) {
    syslog(code==SQLITE_OK?LOG_INFO:LOG_ERR,
        "digit.sqlite3 stage=%s sqlite_code=%d detail=%s",
        stage,code,detail?detail:"none");
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api) {
    int code;
    const char *stage="initial_validation";
    if(database||host||!api||!api->register_service||!api->unregister_service) {
        startup_log(stage,SQLITE_MISUSE,
            "host_service_callbacks_missing_or_already_started");
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }
    stage="open";
    code=sqlite3_open_v2(DB_PATH,&database,
        SQLITE_OPEN_READONLY|SQLITE_OPEN_FULLMUTEX,NULL);
    if(code!=SQLITE_OK) {
        startup_log(stage,code,database?sqlite3_errmsg(database):"no_handle");
        goto fail;
    }
    /* Existing databases only; do not create or migrate schemas. */
    stage="read_only";
    code=sqlite3_db_readonly(database,"main");
    if(code!=1){
        startup_log(stage,SQLITE_READONLY,"database_not_read_only");
        goto fail;
    }
    /* Narrow dedicated RW handle, strictly restricted to observations INSERT.
     * Existing corpus handle remains explicitly read-only. */
    code=sqlite3_open_v2(DB_PATH,&observation_writer,
        SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);
    if(code!=SQLITE_OK)goto fail;
    sqlite3_busy_timeout(observation_writer,1500);
    host=api;
    stage="register_observation";
    if(!host->register_service(DIGIT_OBSERVATION_INSERT_SERVICE,
                               observation_insert,NULL))goto fail;
    stage="register_s2_observations";
    if(!host->register_service(DIGIT_S2_OBSERVATIONS_SERVICE,s2_observations,NULL)){
        host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
        goto fail;
    }
    stage="register_get";
    if(!host->register_service("digit.sqlite3.get",service,(void *)op_get)) {
        startup_log(stage,SQLITE_MISUSE,"core_rejected_service_registration");
        host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
        host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
        goto fail;
    }
    stage="register_corpus_search";
    if(!host->register_service(DIGIT_CORPUS_SEARCH_SERVICE,service,(void *)op_search)){
        host->unregister_service("digit.sqlite3.get",(void *)op_get);
        host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
        host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
        goto fail;
    }
    stage="register_diagnostic";
    if(!host->register_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,
                              diagnostic_service,NULL)){
        startup_log(stage,SQLITE_MISUSE,"core_rejected_diagnostic_registration");
        host->unregister_service(DIGIT_CORPUS_SEARCH_SERVICE,(void *)op_search);
        host->unregister_service("digit.sqlite3.get",(void *)op_get);
        host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
        host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
        goto fail;
    }
    stage="register_observation_recent";
    if(!host->register_service(DIGIT_OBSERVATION_RECENT_SERVICE,
                               observation_recent,NULL)){
        host->unregister_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,NULL);
        host->unregister_service(DIGIT_CORPUS_SEARCH_SERVICE,(void *)op_search);
        host->unregister_service("digit.sqlite3.get",(void *)op_get);
        host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
        host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
        goto fail;
    }
    stage="register_context_store";
    if(!host->register_service(DIGIT_CONTEXT_STORE_SERVICE,context_store,NULL))
        goto fail_registered;
    stage="register_context_recent";
    if(!host->register_service(DIGIT_CONTEXT_RECENT_SERVICE,context_recent,NULL)){
        host->unregister_service(DIGIT_CONTEXT_STORE_SERVICE,NULL);
        goto fail_registered;
    }
    if(!digit_ingest_start(host)){
        startup_log("ingest",SQLITE_ERROR,"watcher_start_failed");
        host->unregister_service(DIGIT_CONTEXT_RECENT_SERVICE,NULL);
        host->unregister_service(DIGIT_CONTEXT_STORE_SERVICE,NULL);
        goto fail_registered;
    }
    startup_log("active",SQLITE_OK,"all_services_registered");
    return STNLABZ_MODULE_OK;
fail_registered:
    host->unregister_service(DIGIT_OBSERVATION_RECENT_SERVICE,NULL);
    host->unregister_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,NULL);
    host->unregister_service(DIGIT_CORPUS_SEARCH_SERVICE,(void *)op_search);
    host->unregister_service("digit.sqlite3.get",(void *)op_get);
    host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
    host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
fail:
    if(database)sqlite3_close(database);
    if(observation_writer)sqlite3_close(observation_writer);
    database=NULL;observation_writer=NULL;host=NULL;
    return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void) {
    if(!database||!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
    digit_ingest_stop();
    host->unregister_service(DIGIT_CONTEXT_RECENT_SERVICE,NULL);
    host->unregister_service(DIGIT_CONTEXT_STORE_SERVICE,NULL);
    host->unregister_service(DIGIT_OBSERVATION_RECENT_SERVICE,NULL);
    host->unregister_service(DIGIT_S2_OBSERVATIONS_SERVICE,NULL);
    host->unregister_service(DIGIT_OBSERVATION_INSERT_SERVICE,NULL);
    host->unregister_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,NULL);
    host->unregister_service(DIGIT_CORPUS_SEARCH_SERVICE,(void *)op_search);
    host->unregister_service("digit.sqlite3.get",(void *)op_get);
    if(sqlite3_close(database)!=SQLITE_OK)return STNLABZ_MODULE_ERR_STOP_FAILED;
    if(observation_writer&&sqlite3_close(observation_writer)!=SQLITE_OK)
      return STNLABZ_MODULE_ERR_STOP_FAILED;
    database=NULL;observation_writer=NULL;host=NULL;return STNLABZ_MODULE_OK;
}
static void test_check(stnlabz_module_qualification_result_t *r,int ok,int negative) {
    r->tests_executed++;
    if(ok)r->tests_passed++;else r->tests_failed++;
    if(negative) {r->negative_test_executed=1;if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *result) {
    sqlite3 *db=NULL;
    sqlite3_record_t record={0};
    sqlite3_diagnostic_t diagnostic={0};
    size_t diagnostic_used=0;
    char key[KEY_MAX]={0};
    int opened;
    if(!result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(result,0,sizeof *result);
    test_check(result,diagnostic_service(NULL,0,&diagnostic,
        sizeof diagnostic,&diagnostic_used,NULL)==STNLABZ_MODULE_OK &&
        diagnostic_used==sizeof diagnostic,0);
    diagnostic_used=1;
    test_check(result,diagnostic_service(NULL,0,&diagnostic,
        sizeof diagnostic-1,&diagnostic_used,NULL)==
        STNLABZ_MODULE_ERR_INVALID_ARGUMENT&&diagnostic_used==0,1);
    test_check(result,diagnostic_service(&diagnostic,1,&diagnostic,
        sizeof diagnostic,&diagnostic_used,NULL)==
        STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
    test_check(result,key_valid("a"),0);
    test_check(result,key_valid("digit.memory/one"),0);
    test_check(result,key_valid("A_Z-9.0"),0);
    test_check(result,!key_valid(NULL),1);
    test_check(result,!key_valid(""),1);
    test_check(result,!key_valid("a b"),1);
    test_check(result,!key_valid("x;DROP TABLE"),1);
    test_check(result,!key_valid("a\\nb"),1);
    test_check(result,!key_valid("a\\rb"),1);
    memset(key,'x',sizeof key);
    test_check(result,!key_valid(key),1);
    /* Fixture exists in RAM only and never opens the live database. */
    opened=sqlite3_open(":memory:",&db)==SQLITE_OK;
    test_check(result,opened,0);
    if(!opened)return STNLABZ_MODULE_ERR_QUALIFICATION;
    test_check(result,sqlite3_exec(db,
       "CREATE TABLE digit_kv(key TEXT PRIMARY KEY,value BLOB NOT NULL) WITHOUT ROWID",
       NULL,NULL,NULL)==SQLITE_OK,0);
    test_check(result,sqlite3_exec(db,
       "INSERT INTO digit_kv VALUES('sample','known value')",
       NULL,NULL,NULL)==SQLITE_OK,0);
    strcpy(record.key,"sample");
    test_check(result,get(db,&record),0);
    test_check(result,record.length==11&&
      !memcmp(record.value,"known value",11),0);
    test_check(result,!get(db,&(sqlite3_record_t){.key="missing"}),1);
    test_check(result,!get(db,&(sqlite3_record_t){.key="bad key"}),1);
    test_check(result,!get(NULL,&record),1);
    test_check(result,!get(db,NULL),1);
    test_check(result,sqlite3_db_readonly(db,"main")==0,0);
    test_check(result,sqlite3_exec(db,
      "CREATE TABLE corpus(id INTEGER PRIMARY KEY,category TEXT,source TEXT,text TEXT,verification_status TEXT,source_record_id TEXT)",NULL,NULL,NULL)==SQLITE_OK,0);
    test_check(result,sqlite3_exec(db,
      "CREATE VIRTUAL TABLE corpus_fts USING fts5(text,content='corpus',content_rowid='id')",NULL,NULL,NULL)==SQLITE_OK,0);
    test_check(result,sqlite3_exec(db,
      "CREATE TRIGGER corpus_fts_ai AFTER INSERT ON corpus BEGIN INSERT INTO corpus_fts(rowid,text) VALUES(new.id,new.text); END",NULL,NULL,NULL)==SQLITE_OK,0);
    test_check(result,sqlite3_exec(db,
      "INSERT INTO corpus(category,text,verification_status) VALUES('test','STN-Labz knowledge record','verified')",NULL,NULL,NULL)==SQLITE_OK,0);
    {
      digit_corpus_query_t query={0};
      digit_corpus_result_t findings={0};
      strcpy(query.query,"STN-Labz");
      test_check(result,corpus_search(db,&query,&findings)==1,0);
      test_check(result,findings.count==1&&findings.hits[0].id==1,0);
      test_check(result,!strcmp(findings.hits[0].verification_status,"verified"),0);
       test_check(result,sqlite3_exec(db,
         "INSERT INTO corpus(category,text,verification_status) "
         "VALUES('LESSON','CSS provides presentation.','unverified')",
         NULL,NULL,NULL)==SQLITE_OK,0);
       strcpy(query.query,"CSS");
       test_check(result,corpus_search(db,&query,&findings)==1,0);
       test_check(result,findings.count==1&&
         !strcmp(findings.hits[0].verification_status,"unverified"),0);
       strcpy(query.query,"STN-Labz");
       test_check(result,corpus_search(db,&query,&findings)==1,0);
      test_check(result,strstr(findings.hits[0].excerpt,"STN-Labz")!=NULL,0);
      strcpy(query.query,"unmatchedrecord");
      test_check(result,corpus_search(db,&query,&findings)==0,0);
      strcpy(query.query,"???");
      test_check(result,corpus_search(db,&query,&findings)==-1,1);
    }

    /* Execute context INSERT/SELECT against the actual schema in RAM,
     * including provenance and deduplication. Never touch the live DB. */
    {
      digit_context_store_request_t request={0};
      digit_context_recent_query_t lookup={0};
      digit_context_recent_result_t found={0};
      size_t used=0;
      sqlite3 *previous_read=database,*previous_writer=observation_writer;
      int rc;
      test_check(result,sqlite3_exec(db,
        "CREATE TABLE conversations(id INTEGER PRIMARY KEY,transport TEXT,channel TEXT);"
        "CREATE TABLE observations(id INTEGER PRIMARY KEY,event_id TEXT,"
        "conversation_id INTEGER,text TEXT,source_record_id TEXT);"
        "CREATE TABLE context(id INTEGER PRIMARY KEY,subject_type TEXT,"
        "subject_id INTEGER,context_type TEXT,value TEXT,source_id TEXT,"
        "confidence REAL,expires_at TEXT);"
        "INSERT INTO conversations VALUES(7,'irc','#stn-labz');"
        "INSERT INTO observations VALUES(1,'irc-event-1',7,'Can you help?','laurie');",
        NULL,NULL,NULL)==SQLITE_OK,0);
      /* Exercise the real atomic Observation -> context writer with
       * the production-shaped schema, not a mocked service. */
      test_check(result,sqlite3_exec(db,
        "ALTER TABLE observations ADD COLUMN sender_id INTEGER;"
        "ALTER TABLE observations ADD COLUMN timestamp TEXT;"
        "CREATE TABLE people(id INTEGER PRIMARY KEY,display_name TEXT);",
        NULL,NULL,NULL)==SQLITE_OK,0);
      database=db;observation_writer=db;
      {
        digit_observation_record_t rec={0};
        sqlite3_stmt *verify=NULL;
        int rc;
        strcpy(rec.transport,"irc");
        strcpy(rec.channel,"#stn-labz");
        strcpy(rec.sender,"UnknownSpeaker");
        strcpy(rec.account,"unknown-account");
        strcpy(rec.text,"Atomic context fixture");
        {
          digit_observation_receipt_t receipt={0};
          used=0;
          test_check(result,observation_insert(&rec,sizeof rec,
            &receipt,sizeof receipt,&used,NULL)==STNLABZ_MODULE_OK &&
            used==sizeof receipt && receipt.observation_id>0 &&
            receipt.context_id>0 && receipt.event_id[0],0);
          used=0;
          test_check(result,observation_insert(&rec,sizeof rec,
            &receipt,sizeof receipt-1,&used,NULL)==
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT && used==0,1);
        }
        rc=sqlite3_prepare_v2(db,
          "SELECT count(*) FROM context x JOIN observations o "
          "ON o.event_id=x.source_id WHERE o.text='Atomic context fixture' "
          "AND x.context_type='utterance.observed' AND x.subject_id=7",
          -1,&verify,NULL);
        test_check(result,rc==SQLITE_OK,0);
        rc=rc==SQLITE_OK?sqlite3_step(verify):SQLITE_ERROR;
        test_check(result,rc==SQLITE_ROW &&
          sqlite3_column_int(verify,0)==1,0);
        if(verify)sqlite3_finalize(verify);
        test_check(result,sqlite3_exec(db,
          "DELETE FROM context WHERE context_type='utterance.observed';"
          "DELETE FROM observations WHERE text='Atomic context fixture';",
          NULL,NULL,NULL)==SQLITE_OK,0);
        strcpy(rec.channel,"#other");
        test_check(result,observation_insert(&rec,sizeof rec,NULL,0,
          &used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      }
      strcpy(request.channel,"#stn-labz");
      strcpy(request.source_account,"laurie");
      strcpy(request.sender,"Laurie");
      strcpy(request.text,"Can you help?");
      request.kind=2;
      rc=context_store(&request,sizeof request,NULL,0,&used,NULL);
      test_check(result,rc==STNLABZ_MODULE_OK&&used==0,0);
      strcpy(lookup.channel,"#stn-labz");
      rc=context_recent(&lookup,sizeof lookup,&found,sizeof found,&used,NULL);
      test_check(result,rc==STNLABZ_MODULE_OK &&
        used==sizeof found && found.count==1,0);
      test_check(result,found.count==1 &&
        !strcmp(found.items[0].value,"Can you help?") &&
        !strcmp(found.items[0].source_id,"irc-event-1") &&
        !strcmp(found.items[0].context_type,"utterance.question"),0);
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_OK,0);
      test_check(result,sqlite3_exec(db,
        "INSERT INTO observations(id,event_id,conversation_id,text,source_record_id) "
        "VALUES(2,'irc-event-2',7,'digicat: Can you help?','laurie')",
        NULL,NULL,NULL)==SQLITE_OK,0);
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_OK,0);
      used=0;
      test_check(result,context_recent(&lookup,sizeof lookup,&found,
        sizeof found,&used,NULL)==STNLABZ_MODULE_OK &&
        found.count==2 &&
        !strcmp(found.items[1].source_id,"irc-event-2"),0);
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_OK,0);
      test_check(result,context_recent(&lookup,sizeof lookup,&found,
        sizeof found,&used,NULL)==STNLABZ_MODULE_OK &&
        found.count==2,0);
      used=0;
      test_check(result,context_recent(&lookup,sizeof lookup,&found,
        sizeof found,&used,NULL)==STNLABZ_MODULE_OK &&
        found.count==2,0);
      strcpy(request.text,"unobserved message");
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_ERR_NOT_FOUND,1);
      request.kind=99;
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      strcpy(request.channel,"#wrong");
      test_check(result,context_store(&request,sizeof request,NULL,0,
        &used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      strcpy(lookup.channel,"#wrong");
      test_check(result,context_recent(&lookup,sizeof lookup,&found,
        sizeof found,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      strcpy(lookup.channel,"#stn-labz");
      test_check(result,sqlite3_exec(db,
        "UPDATE context SET expires_at='2000-01-01 00:00:00'",
        NULL,NULL,NULL)==SQLITE_OK,0);
      used=0;
      test_check(result,context_recent(&lookup,sizeof lookup,&found,
        sizeof found,&used,NULL)==STNLABZ_MODULE_OK &&
        used==sizeof found && found.count==0,0);
      database=previous_read;observation_writer=previous_writer;
    }
    test_check(result,sqlite3_close(db)==SQLITE_OK,0);
    test_check(result,result->tests_executed>=20,0);
    return result->tests_executed>=20 && !result->tests_failed &&
        result->tests_passed==result->tests_executed &&
        result->negative_test_executed&&result->negative_test_passed ?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static const stnlabz_module_descriptor_t descriptor={
    .id="sqlite3",.name="SQLite3 persistence",
    .version_major=0,.version_minor=4,.version_patch=14,
    .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
    .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
    return &descriptor;
}
