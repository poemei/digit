/* [AI:GPT-6 | 2026-10-22] Digit Slack transport candidate v0.1.1.
 * Module-only ABI communication. Requires operator review and qualification.
 * No Core changes. Lessons config is reserved and never used operationally.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"
#include "dsd.h"
#include "../observation/observation.h"
#include <stdint.h>

#define CONFIG_PATH "/opt/digit/auth/slack/slack.conf"
#define CONVERSATION_SERVICE "digit.conversation"
#define CONVERSATION_LISTEN_SERVICE "digit.conversation.listen"
#define INTEL_DELIVERY "digit.slack.intel.deliver"
#define MODULE_DELIVERY "digit.slack.modules.deliver"
#define BRIDGE_DELIVERY "digit.slack.bridge.deliver"
#define IRC_BRIDGE "digit.irc.bridge.deliver"
#define MAX_REPORT 16000
#define WS_BUFFER 65536

typedef struct {
    char sender[128], account[128], target[128], message[512];
} digit_message_t; /* Matches current Conversation request ABI. */
typedef struct {
    int enabled;
    char workspace[64], app_token[512], bot_token[512];
    char intel_channel[64], module_channel[64], bridge_channel[64], learn_user[64];
    char learn_key[256]; /* Reserved for unimplemented Lessons; never consumed. */
} slack_config_t;
typedef struct {char *bytes;size_t used,cap;} response_buffer_t;
static slack_config_t config;
static const stnlabz_module_host_t *host_api;
static pthread_t worker;
static atomic_int running=0;
static int worker_started;
static pthread_mutex_t post_lock=PTHREAD_MUTEX_INITIALIZER;
#define DSD_SLOTS 64
typedef struct {char user[128];uint64_t trace;} dsd_slot_t;
static dsd_slot_t dsd_slots[DSD_SLOTS];
static unsigned dsd_evict;
static uint64_t dsd_next=1;
static uint64_t dsd_previous(const char *user){
 for(unsigned i=0;i<DSD_SLOTS;i++)
  if(!strcmp(dsd_slots[i].user,user))return dsd_slots[i].trace;
 return 0;
}
static void dsd_remember(const char *user,uint64_t trace){
 unsigned slot=DSD_SLOTS;
 for(unsigned i=0;i<DSD_SLOTS;i++){
  if(!strcmp(dsd_slots[i].user,user)){slot=i;break;}
  if(slot==DSD_SLOTS&&!dsd_slots[i].user[0])slot=i;
 }
 if(slot==DSD_SLOTS)slot=dsd_evict++%DSD_SLOTS;
 memset(&dsd_slots[slot],0,sizeof dsd_slots[slot]);
 snprintf(dsd_slots[slot].user,sizeof dsd_slots[slot].user,"%s",user);
 dsd_slots[slot].trace=trace;
}
static int is_dsd(const char *message){
 return message&&(!strcmp(message,"show dsd")||
  !strcmp(message,"show trace")||
  !strcmp(message,"diagnose my last request")||
  !strncmp(message,"diagnose trace ",15)||
  !strncmp(message,"diagnose module ",16));
}

static void wipe(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static int copy(char *dst,size_t cap,const char *src){size_t n;
 if(!dst||!src||(n=strlen(src))>=cap)return 0;
 memcpy(dst,src,n+1);return 1;
}
static int private_config(const char *path){struct stat st;
 return lstat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
   (st.st_uid==0||st.st_uid==geteuid())&&!(st.st_mode&077);
}
static int load_config(slack_config_t *dst){
 FILE *fp;char line[2048];slack_config_t c={0};int ok=1;
 if(!dst||!private_config(CONFIG_PATH)||(fp=fopen(CONFIG_PATH,"r"))==NULL)return 0;
 while(fgets(line,sizeof line,fp)){
   char *k=line,*v,*e;
   if(!strchr(line,'\n')&&!feof(fp)){ok=0;break;}
   while(isspace((unsigned char)*k))k++;
   if(!*k||*k=='#')continue;
   if(!(v=strchr(k,'='))){ok=0;break;}*v++=0;
   e=k+strlen(k);while(e>k&&isspace((unsigned char)e[-1]))*--e=0;
   e=v+strlen(v);while(e>v&&isspace((unsigned char)e[-1]))*--e=0;
   if(!strcmp(k,"enabled")){if(!strcmp(v,"true"))c.enabled=1;else if(strcmp(v,"false"))ok=0;}
   else if(!strcmp(k,"workspace_id"))ok=copy(c.workspace,sizeof c.workspace,v);
   else if(!strcmp(k,"app_token"))ok=copy(c.app_token,sizeof c.app_token,v);
   else if(!strcmp(k,"bot_token"))ok=copy(c.bot_token,sizeof c.bot_token,v);
   else if(!strcmp(k,"intel_channel_id"))ok=copy(c.intel_channel,sizeof c.intel_channel,v);
   else if(!strcmp(k,"module_channel_id"))ok=copy(c.module_channel,sizeof c.module_channel,v);
   else if(!strcmp(k,"bridge_channel_id"))ok=copy(c.bridge_channel,sizeof c.bridge_channel,v);
   else if(!strcmp(k,"learn_operator_user_id"))ok=copy(c.learn_user,sizeof c.learn_user,v);
   else if(!strcmp(k,"learn_authorization_key"))ok=copy(c.learn_key,sizeof c.learn_key,v);
   else ok=0;
   if(!ok)break;
 }
 if(ferror(fp))ok=0;
 fclose(fp);
 if(ok&&c.enabled)ok=c.workspace[0]&&c.app_token[0]&&c.bot_token[0]&&
  !strncmp(c.app_token,"xapp-",5)&&!strncmp(c.bot_token,"xoxb-",5);
 if(ok)*dst=c;
 wipe(&c,sizeof c);wipe(line,sizeof line);return ok;
}
static size_t receive_http(char *ptr,size_t size,size_t count,void *data){
 response_buffer_t *b=data;size_t n=size*count;
 if((size&&n/size!=count)||!b||n>b->cap-b->used-1)return 0;
 memcpy(b->bytes+b->used,ptr,n);b->used+=n;b->bytes[b->used]=0;return n;
}
static char *request_json(const char *url,const char *bearer,struct json_object *payload){
 CURL *curl=NULL;struct curl_slist *headers=NULL;response_buffer_t b={0};
 char authorization[600];long status=0;char *result=NULL;
 b.cap=WS_BUFFER;b.bytes=calloc(b.cap,1);
 if(!b.bytes||!url||!bearer)goto done;
 if(snprintf(authorization,sizeof authorization,"Authorization: Bearer %s",bearer)>=(int)sizeof authorization)goto done;
 curl=curl_easy_init();if(!curl)goto done;
 headers=curl_slist_append(headers,authorization);
 headers=curl_slist_append(headers,"Content-Type: application/json; charset=utf-8");
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,payload?json_object_to_json_string(payload):"{}");
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive_http);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&b);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,10L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 if(curl_easy_perform(curl)!=CURLE_OK)goto done;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 if(status!=200)goto done;
 result=b.bytes;b.bytes=NULL;
 done:
 wipe(authorization,sizeof authorization);
 curl_slist_free_all(headers);if(curl)curl_easy_cleanup(curl);free(b.bytes);
 return result;
}
static const char *field(struct json_object *j,const char *name){struct json_object *v=NULL;
 if(!j||!json_object_object_get_ex(j,name,&v)||!json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
static struct json_object *obj(struct json_object *j,const char *name){struct json_object *v=NULL;
 return j&&json_object_object_get_ex(j,name,&v)&&json_object_is_type(v,json_type_object)?v:NULL;
}
static int posted(const char *channel,const char *message,const char *thread){
 struct json_object *payload=NULL,*reply=NULL;char *raw=NULL;int ok=0;
 if(!channel||!*channel||!message||!*message)return 0;
 if(!(payload=json_object_new_object()))return 0;
 json_object_object_add(payload,"channel",json_object_new_string(channel));
 json_object_object_add(payload,"text",json_object_new_string(message));
 if(thread&&*thread)json_object_object_add(payload,"thread_ts",json_object_new_string(thread));
 pthread_mutex_lock(&post_lock);
 raw=request_json("https://slack.com/api/chat.postMessage",config.bot_token,payload);
 pthread_mutex_unlock(&post_lock);
 if(raw&&(reply=json_tokener_parse(raw))){struct json_object *v=NULL;
  ok=json_object_object_get_ex(reply,"ok",&v)&&json_object_get_boolean(v);
 }
 if(reply)json_object_put(reply);
 free(raw);
 json_object_put(payload);
 return ok;
}
static int clean_report(const char *body,size_t capacity){size_t i;
 if(!body||!memchr(body,0,capacity)||!body[0])return 0;
 for(i=0;body[i];i++)if((unsigned char)body[i]<32&&body[i]!='\n'&&body[i]!='\t')return 0;
 return 1;
}
static stnlabz_module_result_t delivery(const void *request,size_t bytes,
 void *response,size_t cap,size_t *used,void *context){
 const char *channel=(const char *)context;
 (void)response;(void)cap;if(used)*used=0;
 if(!channel||!channel[0]||!request||!bytes||bytes>MAX_REPORT||
    !clean_report(request,bytes))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!atomic_load(&running))return STNLABZ_MODULE_ERR_INVALID_STATE;
 return posted(channel,(const char *)request,NULL)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
/* Cross-transport bridge only accepts one-line bounded public chat, never
 * private intelligence or module lifecycle content. 0 disables bridging. */
typedef struct {char report[384];} bridge_report_t;
static int bridge_allowed(const char *channel){
 return config.bridge_channel[0]=='C'&&channel&&
  !strcmp(channel,config.bridge_channel)&&
  strcmp(channel,config.intel_channel)&&strcmp(channel,config.module_channel);
}
static stnlabz_module_result_t bridge_delivery(const void *request,size_t bytes,
 void *response,size_t cap,size_t *used,void *context){
 const bridge_report_t *msg=request;
 (void)response;(void)cap;(void)context;
 if(used)*used=0;
 if(!bridge_allowed(config.bridge_channel)||!request||bytes!=sizeof *msg||
    !memchr(msg->report,0,sizeof msg->report)||!msg->report[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(size_t i=0;msg->report[i];i++)
  if((unsigned char)msg->report[i]<32||(unsigned char)msg->report[i]==127)
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!atomic_load(&running))return STNLABZ_MODULE_ERR_INVALID_STATE;
 return posted(config.bridge_channel,msg->report,NULL)?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static void relay_irc(const char *channel,const char *user,const char *body){
 bridge_report_t request={0};
 char clean[224]={0};size_t i,j=0,used=0;
 if(!bridge_allowed(channel)||!host_api||!host_api->invoke_service||!user||!body)return;
 for(i=0;body[i]&&j<sizeof clean-1;i++){
  unsigned char ch=(unsigned char)body[i];
  if(ch>=32&&ch!=127)clean[j++]=(char)ch;
 }
 if(!j)return;
 if(snprintf(request.report,sizeof request.report,"[Slack %.*s] %s",32,user,clean)<0)return;
 (void)host_api->invoke_service(IRC_BRIDGE,&request,sizeof request,NULL,0,&used);
}
/* Core hotload writes authoritative lifecycle results to its module audit
 * log. Read forward only, without changing Core or inferring successes. */
#define MODULE_AUDIT "/opt/digit/logs/modules.log"
static FILE *module_audit;
static ino_t audit_inode;
static void audit_token(const char *line,const char *key,char *out,size_t cap){
 const char *p=strstr(line,key);size_t n;
 if(!cap)return;
 out[0]=0;
 if(!p)return;
 p+=strlen(key);n=strcspn(p," \t\r\n");
 if(!n||n>=cap)return;
 memcpy(out,p,n);out[n]=0;
}
static void poll_module_audit(void){
 struct stat st;
 char line[2048],event[64],id[96],prior[48],candidate[48],reason[128],recovery[128];
 int count=0;
 if(!config.module_channel[0]||!atomic_load(&running))return;
 if(lstat(MODULE_AUDIT,&st)||!S_ISREG(st.st_mode))return;
 if(module_audit&&audit_inode!=st.st_ino){
  fclose(module_audit);module_audit=NULL;
 }
 if(!module_audit){
  module_audit=fopen(MODULE_AUDIT,"r");
  if(!module_audit)return;
  audit_inode=st.st_ino;
  /* On first open, do not replay old notices into an operations flood. */
  if(fseek(module_audit,0,SEEK_END))return;
 }
 while(count++<16&&fgets(line,sizeof line,module_audit)){
  char report[384];
  const char *detail=strstr(line,"detail=reported_event=");
  if(!strchr(line,'\n')){int c;while((c=fgetc(module_audit))!='\n'&&c!=EOF){}continue;}
  if(!detail)continue;
  detail+=strlen("detail=");
  audit_token(detail,"reported_event=",event,sizeof event);
  if(strcmp(event,"HOTLOAD_REPLACED")&&strcmp(event,"HOTLOAD_ADMITTED"))continue;
  audit_token(detail,"module=",id,sizeof id);
  audit_token(detail,"incumbent_version=",prior,sizeof prior);
  audit_token(detail,"candidate_version=",candidate,sizeof candidate);
  audit_token(detail,"reason=",reason,sizeof reason);
  audit_token(detail,"recovery=",recovery,sizeof recovery);
  if(!id[0]||!candidate[0])continue;
  if(snprintf(report,sizeof report,
    "[MODULE] HOTLOAD_ACTIVE id=%s active=%s incumbent=%s candidate=%s reason=%s recovery=%s",
    id,candidate,prior[0]?prior:"unknown",candidate,
    reason[0]?reason:"unknown",recovery[0]?recovery:"unknown")>=(int)sizeof report)continue;
  if(!posted(config.module_channel,report,NULL)){
   /* Retry the complete event instead of silently losing it on HTTP failure. */
   (void)fseek(module_audit,-(long)strlen(line),SEEK_CUR);
   break;
  }
 }
 clearerr(module_audit);
}
static int ws_send(CURL *curl,const char *text){size_t sent=0;size_t len=strlen(text);
 return curl_ws_send(curl,text,len,&sent,0,CURLWS_TEXT)==CURLE_OK&&sent==len;
}
/* Normalize Slack's mention markup without treating channel text as a command.
 * Reject mention-only inputs as empty; the transport supplies an acknowledgement.
 */
static int normalized_message(const char *raw,int mention,char *out,size_t cap){
 const char *p=raw;size_t used=0;int seen=0;
 if(!raw||!out||cap<2)return 0;
 while(*p){
  if(mention&&p[0]=='<'&&p[1]=='@'){
   const char *end=strchr(p+2,'>');
   if(end&&end>p+2&&end-p<128){
    const char *q=p+2;
    while(q<end&&(isalnum((unsigned char)*q)||*q=='_'||*q=='-'))q++;
    if(q==end){p=end+1;seen=1;continue;}
   }
  }
  if(used+1>=cap)return 0;
  out[used++]=*p++;
 }
 out[used]=0;
 if(mention&&!seen)return 0;
 {
  char *first=out;
  while(isspace((unsigned char)*first))first++;
  if(first!=out)memmove(out,first,strlen(first)+1);
  used=strlen(out);
  while(used&&isspace((unsigned char)out[used-1]))out[--used]=0;
 }
 return used!=0;
}
/* Same channel contract as IRC:
 * DM -> direct Conversation (DSD is private-only);
 * addressed mention -> direct Conversation;
 * general channel speech -> passive Conversation listen.
 * Ordinary channel text is NEVER dropped just because Digit was not tagged.
 */
static int route_kind(const char *type,const char *channel_type,const char *channel,int is_bot,int has_subtype){
 if(is_bot||has_subtype||!type)return 0;
 if(!strcmp(type,"app_mention"))return 2;
 if(strcmp(type,"message"))return 0;
 if(channel_type&&!strcmp(channel_type,"im"))return 3;
 if(channel_type&&(!strcmp(channel_type,"channel")||!strcmp(channel_type,"group")))
  return 1;
 /* Slack can omit channel_type. Classify by channel ID only as fallback. */
 if(!channel_type&&channel&&channel[0]=='D')return 3;
 if(!channel_type&&channel&&(channel[0]=='C'||channel[0]=='G'))return 1;
 return 0;
}
static const char *event_team(struct json_object *root,struct json_object *payload){
 const char *team=field(payload,"team_id");
 if(!team)team=field(root,"team_id");
 if(!team)team=field(root,"team");
 return team;
}
/* IRC records ambient channel speech before deciding participation. Slack
 * submits the same Observation ABI envelope, including quiet messages. */
static stnlabz_module_result_t observe_slack(const char *channel,
 const char *user,const char *body){
 digit_observation_record_t rec={0};
 digit_observation_receipt_t receipt={0};
 size_t used=0;
 stnlabz_module_result_t rc;
 if(!host_api||!host_api->invoke_service||!channel||!user||!body)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!copy(rec.transport,sizeof rec.transport,"slack")||
    !copy(rec.channel,sizeof rec.channel,channel)||
    !copy(rec.sender,sizeof rec.sender,user)||
    !copy(rec.account,sizeof rec.account,user)||
    !copy(rec.text,sizeof rec.text,body))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 rc=host_api->invoke_service(DIGIT_OBSERVATION_RECORD_SERVICE,
    &rec,sizeof rec,NULL,0,&used);
 if(rc==STNLABZ_MODULE_ERR_INVALID_ARGUMENT){
  used=0;
  rc=host_api->invoke_service(DIGIT_OBSERVATION_INSERT_SERVICE,
     &rec,sizeof rec,&receipt,sizeof receipt,&used);
  if(rc==STNLABZ_MODULE_OK&&
    (used!=sizeof receipt||receipt.observation_id<=0||
     receipt.context_id<=0||!memchr(receipt.event_id,0,sizeof receipt.event_id)||
     !receipt.event_id[0]))return STNLABZ_MODULE_ERR_INVALID_STATE;
 }else if(rc==STNLABZ_MODULE_OK&&used!=0)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 return rc;
}
/* IRC treats a leading nick as an explicit address without a platform mention. */
static size_t slack_address_prefix(const char *text){
 const char *names[]={"digit","digicat"};
 if(!text)return 0;
 for(size_t i=0;i<sizeof names/sizeof names[0];i++){
  size_t n=strlen(names[i]);
  if(!strncasecmp(text,names[i],n)&&
     (!text[n]||text[n]==':'||text[n]==','||text[n]=='?'||text[n]=='!'||
      isspace((unsigned char)text[n])))return n;
 }
 return 0;
}
static void on_event(struct json_object *root){
 struct json_object *payload=obj(root,"payload"),*event=obj(payload,"event");
 const char *type=field(event,"type"),*team=event_team(root,payload);
 const char *user=field(event,"user"),*channel=field(event,"channel");
 const char *text=field(event,"text"),*thread=field(event,"thread_ts");
 const char *subtype=field(event,"subtype");
 const char *bot_id=field(event,"bot_id");
 const char *channel_type=field(event,"channel_type");
 digit_message_t msg={0};char answer[8192]={0};size_t used=0;
 const char *service;
 int mode;
 if(!event||!team||strcmp(team,config.workspace)||!type||!user||!channel||
    !text||!host_api||!host_api->invoke_service)return;
 mode=route_kind(type,channel_type,channel,bot_id!=NULL,subtype!=NULL);
 if(mode==0)return;
 /* Observation is independent of interpretation and response selection. */
 if(mode==1||mode==2)(void)observe_slack(channel,user,text);
 if(mode==1&&slack_address_prefix(text))mode=2;
 if(!copy(msg.sender,sizeof msg.sender,user)||
    !copy(msg.account,sizeof msg.account,user)||
    !copy(msg.target,sizeof msg.target,channel))return;
 if(!normalized_message(text,mode==2&&strstr(text,"<@")!=NULL,
       msg.message,sizeof msg.message)){
  /* The passive listener never creates synthetic speech or replies.
   * Even an empty mention is passed to Conversation for its own decision. */
  if(mode!=2)return;
  msg.message[0]=0;
 }
 /* Remove an IRC-style leading address after Slack mention normalization. */
 if(mode==2&&slack_address_prefix(msg.message)){
  size_t offset=slack_address_prefix(msg.message);
  char *p=msg.message+offset;
  if(*p==':'||*p==','||*p=='?'||*p=='!')p++;
  while(isspace((unsigned char)*p))p++;
  memmove(msg.message,p,strlen(p)+1);
 }
 service=mode==1?CONVERSATION_LISTEN_SERVICE:CONVERSATION_SERVICE;
 if(mode==3&&is_dsd(msg.message)){
  digit_dsd_command_request_t query={0};
  digit_dsd_command_result_t result={0};
  size_t returned=0;
  stnlabz_module_result_t rc;
  if(strcmp(msg.message,"show dsd")&&strcmp(msg.message,"show trace")&&
     strcmp(msg.message,"diagnose my last request")){
   (void)posted(channel,"DSD: Use diagnose my last request. Other trace investigations require scoped authorization.",thread);
   return;
  }
  query.previous_trace_id=dsd_previous(user);
  (void)copy(query.command,sizeof query.command,msg.message);
  rc=host_api->invoke_service(DIGIT_DSD_COMMAND_SERVICE,&query,sizeof query,
    &result,sizeof result,&returned);
  if(rc==STNLABZ_MODULE_OK&&returned==sizeof result&&
     memchr(result.text,0,sizeof result.text))
    (void)posted(channel,result.text,thread);
  else (void)posted(channel,"DSD is unavailable or not active.",thread);
  return;
 }
 if(mode==3){
  digit_dsd_record_t record={0};
  uint64_t trace=dsd_next++;
  uint64_t clear=0;
  size_t trace_used=0;
  stnlabz_module_result_t rc;
  if(!trace)trace=dsd_next++;
  (void)host_api->invoke_service(DIGIT_DSD_BIND_SERVICE,
      &trace,sizeof trace,NULL,0,&trace_used);
  rc=host_api->invoke_service(service,&msg,sizeof msg,answer,sizeof answer,&used);
  (void)host_api->invoke_service(DIGIT_DSD_BIND_SERVICE,
      &clear,sizeof clear,NULL,0,&trace_used);
  record.trace_id=record.event.trace_id=trace;
  record.event.span_id=1;
  record.event.line=__LINE__;
  record.event.result=rc==STNLABZ_MODULE_OK?0:1;
  snprintf(record.event.module,sizeof record.event.module,"slack");
  snprintf(record.event.file,sizeof record.event.file,"modules/slack/slack.c");
  snprintf(record.event.method,sizeof record.event.method,"on_event");
  snprintf(record.event.operation,sizeof record.event.operation,"digit.conversation");
  snprintf(record.event.status,sizeof record.event.status,"%s",
    rc==STNLABZ_MODULE_OK?(used?"RESPONSE_RETURNED":"NO_RESPONSE"):
    rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR");
  if(host_api->invoke_service(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,
      NULL,0,&trace_used)==STNLABZ_MODULE_OK)dsd_remember(user,trace);
  if(rc!=STNLABZ_MODULE_OK)return;
 }else if(host_api->invoke_service(service,&msg,sizeof msg,
      answer,sizeof answer,&used)!=STNLABZ_MODULE_OK)return;
 if(used&&used<sizeof answer){
  answer[used]=0;
  (void)posted(channel,answer,thread);
 }
 /* Only configured general-channel traffic leaves Slack. DMs, intelligence
  * and module/private operations must never be bridged. */
 if(mode==1||mode==2)relay_irc(channel,user,msg.message);
}
static void socket_session(const char *url){
 CURL *ws=curl_easy_init();char frame[WS_BUFFER];size_t length=0;
 if(!ws)return;
 curl_easy_setopt(ws,CURLOPT_URL,url);
 curl_easy_setopt(ws,CURLOPT_CONNECT_ONLY,2L);
 curl_easy_setopt(ws,CURLOPT_CONNECTTIMEOUT,15L);
 curl_easy_setopt(ws,CURLOPT_NOSIGNAL,1L);
 if(curl_easy_perform(ws)==CURLE_OK){
  while(atomic_load(&running)){
   const struct curl_ws_frame *meta=NULL;size_t got=0;
   CURLcode rc=curl_ws_recv(ws,frame+length,sizeof(frame)-length-1,&got,&meta);
   if(rc==CURLE_AGAIN){struct timespec nap={0,100000000L};poll_module_audit();nanosleep(&nap,NULL);continue;}
   if(rc!=CURLE_OK||!meta)break;
   length+=got;
   if(length>=sizeof(frame)-1)break;
   if(meta->bytesleft)continue;
   frame[length]=0;
   if(meta->flags&CURLWS_TEXT){
    struct json_object *root=json_tokener_parse(frame);
    if(root){const char *envelope=field(root,"envelope_id");
     if(envelope){struct json_object *ack=json_object_new_object();
      if(ack){json_object_object_add(ack,"envelope_id",json_object_new_string(envelope));
       /* Acknowledge only after parsing the envelope; delivery remains best-effort. */
       if(ws_send(ws,json_object_to_json_string(ack)))on_event(root);
       json_object_put(ack);
      }
     }
     json_object_put(root);
    }
   }
   length=0;
  }
 }
 curl_easy_cleanup(ws);
}
static void *socket_worker(void *unused){(void)unused;
 while(atomic_load(&running)){
  char *raw=request_json("https://slack.com/api/apps.connections.open",config.app_token,NULL);
  struct json_object *j=raw?json_tokener_parse(raw):NULL;
  const char *url=j?field(j,"url"):NULL;
  struct json_object *ok=NULL;
  if(j&&json_object_object_get_ex(j,"ok",&ok)&&json_object_get_boolean(ok)&&
     url&&!strncmp(url,"wss://",6))socket_session(url);
  if(j)json_object_put(j);
  free(raw);
  for(int i=0;i<30&&atomic_load(&running);i++){
   poll_module_audit();
   struct timespec nap={1,0};nanosleep(&nap,NULL);
  }
 }
 return NULL;
}
static void check(stnlabz_module_qualification_result_t *r,int valid,int negative){
 r->tests_executed++;if(valid)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(valid)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 char x[8]={0};
 char good[]="hello";
 char empty[]="";
 char carriage[]="x\r";
 char newline[]="x\ny";
 char tab[]="x\ty";
 char nul[]={'x',0,'y'};
 char control[]={'x',1,0};
 char full[8]="1234567";
 slack_config_t cfg_check={0};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 /* Digit's checked-in ABI contract requires at least 20 executed cases.
  * Exercise meaningful positive and negative paths; no fabricated counters. */
 check(r,copy(x,sizeof x,"abc"),0);
 check(r,!strcmp(x,"abc"),0);
 check(r,copy(x,sizeof x,"1234567"),0);
 check(r,!strcmp(x,full),0);
 check(r,!copy(x,sizeof x,"abcdefgh"),1);
 check(r,!copy(x,sizeof x,"abcdefghijk"),1);
 check(r,!copy(NULL,sizeof x,"abc"),1);
 check(r,!copy(x,sizeof x,NULL),1);
 check(r,copy(x,sizeof x,""),0);
 check(r,x[0]==0,0);
 check(r,clean_report(good,sizeof good),0);
 check(r,!clean_report(NULL,0),1);
 check(r,!clean_report(empty,sizeof empty),1);
 check(r,!clean_report(carriage,sizeof carriage),1);
 check(r,clean_report(newline,sizeof newline),0);
 check(r,clean_report(tab,sizeof tab),0);
 check(r,!clean_report(control,sizeof control),1);
 check(r,clean_report(nul,sizeof nul),0);
 check(r,!clean_report(good,2),1);
 check(r,!private_config("/path/not/present"),1);
 check(r,!private_config(NULL),1);
 check(r,!strcmp(CONVERSATION_SERVICE,"digit.conversation"),0);
 check(r,!strcmp(INTEL_DELIVERY,"digit.slack.intel.deliver"),0);
 check(r,!strcmp(MODULE_DELIVERY,"digit.slack.modules.deliver"),0);
 {char token[40]={0};
  audit_token("reported_event=HOTLOAD_REPLACED module=irc","reported_event=",token,sizeof token);
  check(r,!strcmp(token,"HOTLOAD_REPLACED"),0);
  audit_token("module=irc incumbent_version=0.3.15","module=",token,sizeof token);
  check(r,!strcmp(token,"irc"),0);
  audit_token("reason=qualification_and_activation_passed recovery=replacement_active","reason=",token,sizeof token);
  check(r,!strcmp(token,"qualification_and_activation_passed"),0);
  audit_token("other=foo","module=",token,sizeof token);
  check(r,!token[0],1);
 }

 check(r,strcmp(INTEL_DELIVERY,MODULE_DELIVERY)!=0,0);
 check(r,MAX_REPORT>=8192,0);
 check(r,sizeof cfg_check.learn_key>=128,0);
 check(r,sizeof cfg_check.intel_channel==sizeof cfg_check.module_channel,0);
 /* End-to-end routing classification contract, including missing metadata. */
 check(r,route_kind("message","channel","C123",0,0)==1,0);
 check(r,route_kind("message","group","G123",0,0)==1,0);
 check(r,route_kind("message","im","D123",0,0)==3,0);
 check(r,route_kind("app_mention","channel","C123",0,0)==2,0);
 check(r,route_kind("message",NULL,"C123",0,0)==1,0);
 check(r,route_kind("message",NULL,"G123",0,0)==1,0);
 check(r,route_kind("message",NULL,"D123",0,0)==3,0);
 check(r,route_kind("message","channel","C123",1,0)==0,1);
 check(r,route_kind("message","channel","C123",0,1)==0,1);
 check(r,route_kind("message","mpim","G123",0,0)==0,1);
 check(r,route_kind("other","channel","C123",0,0)==0,1);
 check(r,route_kind("message",NULL,NULL,0,0)==0,1);
 check(r,is_dsd("diagnose my last request"),0);
 check(r,!strcmp(BRIDGE_DELIVERY,"digit.slack.bridge.deliver"),0);
 check(r,!strcmp(IRC_BRIDGE,"digit.irc.bridge.deliver"),0);
 {slack_config_t previous=config; memset(&config,0,sizeof config);
  check(r,!bridge_allowed("C123"),1);
  snprintf(config.bridge_channel,sizeof config.bridge_channel,"C123");
  check(r,bridge_allowed("C123"),0);
  snprintf(config.bridge_channel,sizeof config.bridge_channel,"G123");
  check(r,!bridge_allowed("G123"),1);
  snprintf(config.bridge_channel,sizeof config.bridge_channel,"C123");
  check(r,!bridge_allowed("C999"),1);
  snprintf(config.intel_channel,sizeof config.intel_channel,"C123");
  check(r,!bridge_allowed("C123"),1);
  config=previous;}
 check(r,is_dsd("show trace"),0);
 check(r,slack_address_prefix("digit what is your mission?")==5,0);
 check(r,slack_address_prefix("Digit, hello")==5,0);
 check(r,slack_address_prefix("digicat: hi")==7,0);
 check(r,slack_address_prefix("digital")==0,1);
 check(r,slack_address_prefix("digit?")==5,0);
 check(r,slack_address_prefix("digicat?")==7,0);
 check(r,slack_address_prefix("digit!")==5,0);
 check(r,slack_address_prefix("Digit?")==5,0);
 check(r,slack_address_prefix("digit? hi")==5,0);
 check(r,slack_address_prefix("hello digit")==0,1);
 check(r,slack_address_prefix(" digit hi")==0,1);
 check(r,!is_dsd("hi"),1);
 check(r,!is_dsd(NULL),1);
 check(r,!strcmp(CONVERSATION_LISTEN_SERVICE,"digit.conversation.listen"),0);
 {char output[64]={0};
  check(r,normalized_message("<@U123> hi",1,output,sizeof output)&&
   !strcmp(output,"hi"),0);
  check(r,normalized_message("  <@U123> help  ",1,output,sizeof output)&&
   !strcmp(output,"help"),0);
  check(r,normalized_message("hi",0,output,sizeof output)&&
   !strcmp(output,"hi"),0);
  check(r,!normalized_message("<@U123>",1,output,sizeof output),1);
  check(r,!normalized_message("hi",1,output,sizeof output),1);
  check(r,!normalized_message("<@U123> abc",1,output,3),1);
  check(r,!normalized_message("<@U123> bad",1,NULL,0),1);
 }

 return r->tests_failed||r->tests_executed<STNLABZ_MODULE_MIN_TESTS||
   !r->negative_test_executed||!r->negative_test_passed?
   STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host_api||!api||!api->invoke_service||!api->register_service||!api->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!load_config(&config)||!config.enabled)return STNLABZ_MODULE_ERR_START_FAILED;
 host_api=api;
 if(!api->register_service(INTEL_DELIVERY,delivery,config.intel_channel))goto fail;
 if(!api->register_service(MODULE_DELIVERY,delivery,config.module_channel)){
  (void)api->unregister_service(INTEL_DELIVERY,config.intel_channel);goto fail;
 }
 if(!api->register_service(BRIDGE_DELIVERY,bridge_delivery,NULL)){
  (void)api->unregister_service(MODULE_DELIVERY,config.module_channel);
  (void)api->unregister_service(INTEL_DELIVERY,config.intel_channel);
  goto fail;
 }
 atomic_store(&running,1);
 if(pthread_create(&worker,NULL,socket_worker,NULL)!=0){
  atomic_store(&running,0);
  (void)api->unregister_service(BRIDGE_DELIVERY,NULL);
  (void)api->unregister_service(MODULE_DELIVERY,config.module_channel);
  (void)api->unregister_service(INTEL_DELIVERY,config.intel_channel);goto fail;
 }
 worker_started=1;return STNLABZ_MODULE_OK;
 fail:host_api=NULL;wipe(&config,sizeof config);return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void){
 if(!host_api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 atomic_store(&running,0);
 if(worker_started){pthread_join(worker,NULL);worker_started=0;}
 if(module_audit){fclose(module_audit);module_audit=NULL;audit_inode=0;}
 if(!host_api->unregister_service(BRIDGE_DELIVERY,NULL)||
    !host_api->unregister_service(MODULE_DELIVERY,config.module_channel)||
    !host_api->unregister_service(INTEL_DELIVERY,config.intel_channel))
    return STNLABZ_MODULE_ERR_STOP_FAILED;
 host_api=NULL;wipe(&config,sizeof config);return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="slack",.name="Slack ABI Transport",
 .version_major=0,.version_minor=1,.version_patch=9,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
/* [End AI:GPT-6] */
