#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "corpus_store.h"
#include "corpus_lookup.h"
#include "corpus_learn.h"
static const stnlabz_module_host_t *host;
static stnlabz_module_result_t search(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *ctx,void *unused){
 digit_corpus_record_t r;char msg[4608];int result;
 (void)unused;
 if(!host||!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result=digit_corpus_find(DIGIT_CORPUS_PATH,cmd->arguments,&r);
 if(result<0)return reply(ctx,"Corpus unavailable or invalid query.")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!result)return reply(ctx,"Corpus: no matching knowledge record.")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(msg,sizeof msg,"Corpus: %s [id=%s category=%s source=%s]",r.text,r.id,r.category,r.source);
 return reply(ctx,msg)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
/* Shared validated append; callers must independently authorize entry. */
static stnlabz_module_result_t append_learn(const char *arguments,
 stnlabz_module_command_reply_fn reply,void *ctx){
 char fields[3][512]={{0}},line[1024],id[65],msg[160];
 unsigned char random_bytes[16];size_t i,n=0,j=0,k=0,total;
 int fd=-1,rng=-1,ok=0;struct stat st;
 const char *p;
 if(!arguments||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 p=arguments;
 /* learn CATEGORY | SOURCE | TEXT : one append-only TSV record. */
 for(;*p;p++){
  unsigned char c=(unsigned char)*p;
  if(c=='|'&&k<2){while(j&&fields[k][j-1]==' ')fields[k][--j]=0;k++;j=0;continue;}
  if(c=='\t'||c=='\n'||c=='\r'||c<32||c==127||j>=sizeof fields[0]-1)
   return reply(ctx,"Learn rejected: invalid field.")?STNLABZ_MODULE_ERR_INVALID_ARGUMENT:STNLABZ_MODULE_ERR_INVALID_STATE;
  fields[k][j++]=*p;
 }
 if(k!=2)return reply(ctx,"Usage: learn CATEGORY | SOURCE | TEXT")?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<3;i++){
  char *start=fields[i];size_t len;
  while(*start==' ')start++;
  if(start!=fields[i])memmove(fields[i],start,strlen(start)+1);
  len=strlen(fields[i]);
  while(len&&fields[i][len-1]==' ')fields[i][--len]=0;
  if(!len|| (i==0&&len>=DIGIT_CORPUS_CATEGORY_MAX)||
    (i==1&&len>=DIGIT_CORPUS_SOURCE_MAX)||
    (i==2&&len>=DIGIT_CORPUS_TEXT_MAX))
   return reply(ctx,"Learn rejected: missing or oversized field.")?STNLABZ_MODULE_ERR_INVALID_ARGUMENT:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 rng=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
 if(rng<0)return reply(ctx,"Learn failed: record identifier unavailable.")?STNLABZ_MODULE_ERR_INVALID_STATE:STNLABZ_MODULE_ERR_INVALID_STATE;
 while(n<sizeof random_bytes){
  ssize_t got=read(rng,random_bytes+n,sizeof random_bytes-n);
  if(got<=0){close(rng);return STNLABZ_MODULE_ERR_INVALID_STATE;}
  n+=(size_t)got;
 }
 close(rng);
 for(i=0;i<sizeof random_bytes;i++)snprintf(id+i*2,3,"%02x",random_bytes[i]);
 id[32]=0;
 total=(size_t)snprintf(line,sizeof line,"LEARN-%s\t%s\t%s\t%s\n",
  id,fields[0],fields[1],fields[2]);
 if(total>=sizeof line)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 fd=open(DIGIT_CORPUS_PATH,O_WRONLY|O_APPEND|O_CLOEXEC|O_NOFOLLOW);
 if(fd>=0&&fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&flock(fd,LOCK_EX)==0){
  size_t written=0;
  while(written<total){
   ssize_t count=write(fd,line+written,total-written);
   if(count<0&&errno==EINTR)continue;
   if(count<=0)break;
   written+=(size_t)count;
  }
  ok=(written==total&&fsync(fd)==0);
  (void)flock(fd,LOCK_UN);
 }
 if(fd>=0)close(fd);
 if(!ok)return reply(ctx,"Learn failed: Corpus append unavailable; inspect file integrity.")?
   STNLABZ_MODULE_ERR_INVALID_STATE:STNLABZ_MODULE_ERR_INVALID_STATE;
 (void)snprintf(msg,sizeof msg,"Learn appended: LEARN-%s",id);
 return reply(ctx,msg)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
/* Keep console authorization unchanged. */
static stnlabz_module_result_t learn(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *ctx,void *unused){
 (void)unused;
 if(!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(strcmp(cmd->sender,"local-console")||strcmp(cmd->account,"local"))
  return reply(ctx,"Learn denied: native CLI only.")?STNLABZ_MODULE_ERR_NOT_AUTHORIZED:STNLABZ_MODULE_ERR_INVALID_STATE;
 return append_learn(cmd->arguments,reply,ctx);
}
static int capture_learn_reply(void *ctx,const char *message){
 digit_corpus_learn_response_t *r=ctx;
 if(!r||!message||strlen(message)>=sizeof r->message)return 0;
 snprintf(r->message,sizeof r->message,"%s",message);return 1;
}
/* Exact locally held credential and operator identity required, even if the
 * Slack transport claims that it already authorized the request. */
static int slack_learn_authorized(const digit_corpus_learn_request_t *r){
 FILE *fp;struct stat st;char line[1024],user[64]={0},key[65]={0},team[64]={0};
 int valid=1;const char *path="/opt/digit/auth/slack/slack.conf";
 if(lstat(path,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||
    (st.st_mode&077)||(st.st_uid!=0&&st.st_uid!=geteuid()))return 0;
 fp=fopen(path,"r");if(!fp)return 0;
 while(fgets(line,sizeof line,fp)){
  char *v=strchr(line,'='),*end;
  if(!v)continue;*v++=0;
  end=v+strlen(v);while(end>v&&(end[-1]=='\n'||end[-1]=='\r'))*--end=0;
  if(!strcmp(line,"learn_operator_user_id")){
   if(!*v||strlen(v)>=sizeof user){valid=0;break;}strcpy(user,v);
  }else if(!strcmp(line,"learn_authorization_key")){
   if(strlen(v)!=64||strspn(v,"0123456789abcdefABCDEF")!=64){valid=0;break;}strcpy(key,v);
  }else if(!strcmp(line,"workspace_id")){
   if(!*v||strlen(v)>=sizeof team){valid=0;break;}strcpy(team,v);
  }
 }
 if(ferror(fp))valid=0;fclose(fp);
 if(!valid||!user[0]||!key[0]||!team[0]||
    strcmp(user,r->user)||strcmp(team,r->workspace))return 0;
 {unsigned diff=0;size_t i;
  for(i=0;i<64;i++)diff|=(unsigned)(unsigned char)(key[i]^r->key[i]);
  return diff==0;
 }
}
static stnlabz_module_result_t slack_learn(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *context){
 const digit_corpus_learn_request_t *r=request;
 digit_corpus_learn_response_t *out=response;
 stnlabz_module_result_t rc;
 (void)context;
 if(used)*used=0;
 if(!host||!r||size!=sizeof *r||!out||capacity<sizeof *out||!used||
    r->version!=DIGIT_CORPUS_LEARN_VERSION||
    !memchr(r->workspace,0,sizeof r->workspace)||
    !memchr(r->channel,0,sizeof r->channel)||
    !memchr(r->user,0,sizeof r->user)||
    !memchr(r->key,0,sizeof r->key)||
    !memchr(r->arguments,0,sizeof r->arguments)||!r->arguments[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(strcmp(r->channel,DIGIT_CORPUS_OPS_CHANNEL)||!slack_learn_authorized(r)){
  if(host->send_message)host->send_message("[CORPUS] Slack learn denied");
  return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
 }
 memset(out,0,sizeof *out);out->version=DIGIT_CORPUS_LEARN_VERSION;
 rc=append_learn(r->arguments,capture_learn_reply,out);
 if(host->send_message)host->send_message(rc==STNLABZ_MODULE_OK?
    "[CORPUS] Slack learn appended":"[CORPUS] Slack learn rejected or failed");
 *used=sizeof *out;
 return rc;
}
/* Corpus owns lookup and source attribution; Ask receives only bounded evidence. */
static stnlabz_module_result_t lookup(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *context){
 const digit_corpus_lookup_request_t *q=request;
 digit_corpus_lookup_response_t *out=response;
 digit_corpus_record_t record;int result;
 (void)context;
 if(!host||!q||size!=sizeof *q||!out||capacity<sizeof *out||!used||
    !memchr(q->term,0,sizeof q->term)||!q->term[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result=digit_corpus_find(DIGIT_CORPUS_PATH,q->term,&record);
 if(result<0)return STNLABZ_MODULE_ERR_INVALID_STATE;
 memset(out,0,sizeof *out);
 if(result>0){
  out->found=1;
  snprintf(out->id,sizeof out->id,"%s",record.id);
  snprintf(out->source,sizeof out->source,"%s",record.source);
  snprintf(out->text,sizeof out->text,"%s",record.text);
 }
 *used=sizeof *out;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 return digit_corpus_selftest(q);
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_command||!h->unregister_command||
    !h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_command("corpus",search,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_command("learn",learn,NULL)){
  (void)h->unregister_command("corpus",NULL);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!h->register_service("corpus.lookup",lookup,NULL)){
  (void)h->unregister_command("learn",NULL);
  (void)h->unregister_command("corpus",NULL);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!h->register_service(DIGIT_CORPUS_LEARN_SERVICE,slack_learn,NULL)){
  (void)h->unregister_service("corpus.lookup",NULL);
  (void)h->unregister_command("learn",NULL);
  (void)h->unregister_command("corpus",NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_CORPUS_LEARN_SERVICE,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service("corpus.lookup",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_command("learn",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_command("corpus",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"corpus");
 (void)snprintf(d.name,sizeof d.name,"Corpus");
 d.version_major=0;d.version_minor=1;d.version_patch=4;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
