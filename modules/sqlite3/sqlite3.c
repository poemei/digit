/* [AI:GPT-6 | 2026-10-18] Digit SQLite3 persistence module, ABI 1.5.
 * The database is owned by this module; Core owns lifecycle and authorization.
 */
#define _POSIX_C_SOURCE 200809L
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include "module.h"
#include "sqlite3_diagnostic.h"

#define DB_PATH "/opt/digit/data/digit_memory.db"
#define KEY_MAX 128
#define VALUE_MAX 1024

typedef struct { char key[KEY_MAX]; unsigned char value[VALUE_MAX]; size_t length; } sqlite3_record_t;
static sqlite3 *database;
static const stnlabz_module_host_t *host;

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
static int schema(sqlite3 *db) {
    return sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS digit_kv ("
        "key TEXT PRIMARY KEY NOT NULL,"
        "value BLOB NOT NULL) WITHOUT ROWID",NULL,NULL,NULL)==SQLITE_OK;
}
static int put(sqlite3 *db,const sqlite3_record_t *r) {
    sqlite3_stmt *stmt=NULL;int ok=0;
    if (!db || !r || !key_valid(r->key) || r->length>VALUE_MAX) return 0;
    if(sqlite3_prepare_v2(db,
        "INSERT INTO digit_kv(key,value) VALUES(?1,?2) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        -1,&stmt,NULL)!=SQLITE_OK) return 0;
    if(sqlite3_bind_text(stmt,1,r->key,-1,SQLITE_TRANSIENT)==SQLITE_OK &&
       sqlite3_bind_blob(stmt,2,r->value,(int)r->length,SQLITE_TRANSIENT)==SQLITE_OK)
        ok=sqlite3_step(stmt)==SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
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
static int erase(sqlite3 *db,const char *key) {
    sqlite3_stmt *stmt=NULL;int ok=0;
    if(!db||!key_valid(key))return 0;
    if(sqlite3_prepare_v2(db,"DELETE FROM digit_kv WHERE key=?1",
                          -1,&stmt,NULL)!=SQLITE_OK)return 0;
    if(sqlite3_bind_text(stmt,1,key,-1,SQLITE_TRANSIENT)==SQLITE_OK)
        ok=sqlite3_step(stmt)==SQLITE_DONE;
    sqlite3_finalize(stmt);return ok;
}
static stnlabz_module_result_t service(const void *request,size_t request_size,
    void *response,size_t response_size,size_t *response_used,void *context) {
    const char *operation=context;
    sqlite3_record_t r;
    if(response_used)*response_used=0;
    if(!database||!operation||!request)return STNLABZ_MODULE_ERR_INVALID_STATE;
    if(!strcmp(operation,"put")) {
        if(request_size!=sizeof r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        return put(database,(const sqlite3_record_t *)request) ?
            STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }
    if(!strcmp(operation,"get")) {
        if(request_size!=KEY_MAX||!response||response_size<sizeof r||
           !response_used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        memset(&r,0,sizeof r);
        memcpy(r.key,request,KEY_MAX);
        if(!get(database,&r))return STNLABZ_MODULE_ERR_NOT_FOUND;
        memcpy(response,&r,sizeof r);*response_used=sizeof r;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(operation,"delete")) {
        if(request_size!=KEY_MAX)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        char key[KEY_MAX];
        memcpy(key,request,sizeof key);
        return erase(database,key)?STNLABZ_MODULE_OK:
            STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
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
      "SELECT name FROM sqlite_master WHERE type='table' AND name='digit_kv'",
      -1,&stmt,NULL);
   if(code!=SQLITE_OK){
      snprintf(out.state,sizeof out.state,"FAILED");
      snprintf(out.stage,sizeof out.stage,"schema_prepare");
      snprintf(out.detail,sizeof out.detail,"%.80s",sqlite3_errstr(code));
   }else{
      step_code=sqlite3_step(stmt);
      if(step_code==SQLITE_ROW){
       snprintf(out.state,sizeof out.state,"OBSERVED");
       snprintf(out.stage,sizeof out.stage,"schema_lookup");
       snprintf(out.detail,sizeof out.detail,"digit_kv table found; data operations not tested");
      }else if(step_code==SQLITE_DONE){
       snprintf(out.state,sizeof out.state,"FAILED");
       snprintf(out.stage,sizeof out.stage,"schema_lookup");
       snprintf(out.detail,sizeof out.detail,"digit_kv table not found");
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
static const char op_put[]="put",op_get[]="get",op_delete[]="delete";
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
    if(database||!api||!api->register_service||!api->unregister_service) {
        startup_log(stage,SQLITE_MISUSE,
            "host_service_callbacks_missing_or_already_started");
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    }
    stage="open";
    code=sqlite3_open_v2(DB_PATH,&database,
        SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);
    if(code!=SQLITE_OK) {
        startup_log(stage,code,database?sqlite3_errmsg(database):"no_handle");
        goto fail;
    }
    stage="schema";
    code=sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS digit_kv ("
        "key TEXT PRIMARY KEY NOT NULL,"
        "value BLOB NOT NULL) WITHOUT ROWID",NULL,NULL,NULL);
    if(code!=SQLITE_OK) {
        startup_log(stage,code,sqlite3_errmsg(database));
        goto fail;
    }
    host=api;
    stage="register_put";
    if(!host->register_service("digit.sqlite3.put",service,(void *)op_put)) {
        startup_log(stage,SQLITE_MISUSE,"core_rejected_service_registration");
        goto fail;
    }
    stage="register_get";
    if(!host->register_service("digit.sqlite3.get",service,(void *)op_get)) {
        startup_log(stage,SQLITE_MISUSE,"core_rejected_service_registration");
        host->unregister_service("digit.sqlite3.put",(void *)op_put);
        goto fail;
    }
    stage="register_delete";
    if(!host->register_service("digit.sqlite3.delete",service,(void *)op_delete)) {
        startup_log(stage,SQLITE_MISUSE,"core_rejected_service_registration");
        host->unregister_service("digit.sqlite3.get",(void *)op_get);
        host->unregister_service("digit.sqlite3.put",(void *)op_put);
        goto fail;
    }
    stage="register_diagnostic";
    if(!host->register_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,
                              diagnostic_service,NULL)){
        startup_log(stage,SQLITE_MISUSE,"core_rejected_diagnostic_registration");
        host->unregister_service("digit.sqlite3.delete",(void *)op_delete);
        host->unregister_service("digit.sqlite3.get",(void *)op_get);
        host->unregister_service("digit.sqlite3.put",(void *)op_put);
        goto fail;
    }
    startup_log("active",SQLITE_OK,"all_services_registered");
    return STNLABZ_MODULE_OK;
fail:
    if(database)sqlite3_close(database);
    database=NULL;host=NULL;
    return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void) {
    if(!database||!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
    host->unregister_service(DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,NULL);
    host->unregister_service("digit.sqlite3.delete",(void *)op_delete);
    host->unregister_service("digit.sqlite3.get",(void *)op_get);
    host->unregister_service("digit.sqlite3.put",(void *)op_put);
    if(sqlite3_close(database)!=SQLITE_OK)return STNLABZ_MODULE_ERR_STOP_FAILED;
    database=NULL;host=NULL;return STNLABZ_MODULE_OK;
}
static void test_check(stnlabz_module_qualification_result_t *r,int ok,int negative) {
    r->tests_executed++;
    if(ok)r->tests_passed++;else r->tests_failed++;
    if(negative) {r->negative_test_executed=1;if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *result) {
    sqlite3 *db=NULL;
    sqlite3_record_t r={0},saved={0};
    char key[KEY_MAX]={0};
    int opened;
    if(!result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(result,0,sizeof *result);
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
    opened=sqlite3_open(":memory:",&db)==SQLITE_OK;
    test_check(result,opened,0);
    if(!opened) return STNLABZ_MODULE_ERR_QUALIFICATION;
    test_check(result,schema(db),0);
    test_check(result,schema(db),0);
    strcpy(r.key,"test.one");r.value[0]=0;r.value[1]=255;r.length=2;
    test_check(result,put(db,&r),0);
    strcpy(saved.key,"test.one");
    test_check(result,get(db,&saved),0);
    test_check(result,saved.length==2&&saved.value[0]==0&&saved.value[1]==255,0);
    test_check(result,!get(db,&(sqlite3_record_t){.key="missing"}),1);
    test_check(result,!put(db,&(sqlite3_record_t){.key="bad key",.length=1}),1);
    test_check(result,!put(db,&(sqlite3_record_t){.key="overflow",.length=VALUE_MAX+1}),1);
    test_check(result,erase(db,"test.one"),0);
    test_check(result,!get(db,&saved),1);
    test_check(result,!erase(db,"bad key"),1);
    test_check(result,!put(NULL,&r),1);
    test_check(result,!get(NULL,&saved),1);
    test_check(result,!erase(NULL,"test.one"),1);
    test_check(result,sqlite3_close(db)==SQLITE_OK,0);
    return result->tests_executed>=20 && !result->tests_failed &&
        result->tests_passed==result->tests_executed &&
        result->negative_test_executed&&result->negative_test_passed ?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static const stnlabz_module_descriptor_t descriptor={
    .id="sqlite3",.name="SQLite3 persistence",
    .version_major=0,.version_minor=1,.version_patch=3,
    .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
    .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
    return &descriptor;
}
