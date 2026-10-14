#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdatomic.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "stnlabz_abi/module.h"
#include "router.h"
#include "ask_context.h"
#include "intel_delivery.h"

#define CONFIG_PATH "/opt/digit/auth/slack/slack.conf"
#define RESPONSE_MAX 65536
#define HISTORY_SIZE 64
typedef struct {char workspace[64],app[512],bot[512],channel[64],operator[64];int enabled;} config_t;
typedef struct {char data[RESPONSE_MAX];size_t size;} response_t;
typedef struct {char user[64],channel[64];digit_scoped_diagnostic_t previous;time_t seen;} history_t;
static config_t cfg;
static const stnlabz_module_host_t *host;
static pthread_t worker;
static atomic_int stopping;
static int active;
static char bot_id[64];
static history_t history[HISTORY_SIZE];
static char event_ids[128][128];
static unsigned int event_cursor;
static char message_keys[128][192];
static unsigned int message_cursor;
static unsigned long request_sequence;
static void erase(void *p,size_t n){volatile unsigned char *q=p;while(n--)*q++=0;}
static void log_status(const char *message){if(host&&host->send_message)(void)host->send_message(message);}
static int private_file(const char *path){
 struct stat s;
 return lstat(path,&s)==0&&S_ISREG(s.st_mode)&&s.st_nlink==1&&
        !(s.st_mode&077)&&(s.st_uid==0||s.st_uid==geteuid());
}
static int copy(char *out,size_t cap,const char *in){
 size_t n;if(!in)return 0;n=strlen(in);
 if(!n||n>=cap)return 0;
 memcpy(out,in,n+1);return 1;
}
static int configuration(void){
 FILE *fp;char line[2048];config_t found={0};int ok=1;
 if(!private_file(CONFIG_PATH))return 0;
 fp=fopen(CONFIG_PATH,"r");if(!fp)return 0;
 while(fgets(line,sizeof line,fp)){
  char *key=line,*value,*end;
  if(!strchr(line,'\n')&&!feof(fp)){ok=0;break;}
  while(isspace((unsigned char)*key))key++;
  if(!*key||*key=='#')continue;
  value=strchr(key,'=');if(!value){ok=0;break;}*value++=0;
  end=key+strlen(key);while(end>key&&isspace((unsigned char)end[-1]))*--end=0;
  end=value+strlen(value);while(end>value&&isspace((unsigned char)end[-1]))*--end=0;
  if(!strcmp(key,"enabled")){if(!strcmp(value,"true"))found.enabled=1;else if(strcmp(value,"false"))ok=0;}
  else if(!strcmp(key,"workspace_id"))ok=copy(found.workspace,sizeof found.workspace,value);
  else if(!strcmp(key,"app_token"))ok=copy(found.app,sizeof found.app,value);
  else if(!strcmp(key,"bot_token"))ok=copy(found.bot,sizeof found.bot,value);
  else if(!strcmp(key,"intel_channel_id"))ok=copy(found.channel,sizeof found.channel,value);
  else if(!strcmp(key,"operator_user_id"))ok=copy(found.operator,sizeof found.operator,value);
  else ok=0;
  if(!ok)break;
 }
 if(ferror(fp))ok=0;
 fclose(fp);
 if(ok)ok=found.enabled&&found.workspace[0]&&found.channel[0]&&
          !strncmp(found.app,"xapp-",5)&&!strncmp(found.bot,"xoxb-",5);
 if(ok)cfg=found;
 erase(&found,sizeof found);erase(line,sizeof line);
 return ok;
}
static const char *field(struct json_object *o,const char *name){
 struct json_object *v=NULL;
 if(!o||!json_object_object_get_ex(o,name,&v)||!json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
static struct json_object *obj(struct json_object *o,const char *name){
 struct json_object *v=NULL;
 if(!o||!json_object_object_get_ex(o,name,&v)||!json_object_is_type(v,json_type_object))return NULL;
 return v;
}
static int success(struct json_object *o){
 struct json_object *v=NULL;
 return o&&json_object_object_get_ex(o,"ok",&v)&&json_object_is_type(v,json_type_boolean)&&json_object_get_boolean(v);
}
static size_t capture(char *bytes,size_t size,size_t count,void *ctx){
 response_t *r=ctx;size_t n;
 if(count&&size>SIZE_MAX/count)return 0;
 n=size*count;
 if(n>sizeof r->data-1-r->size)return 0;
 memcpy(r->data+r->size,bytes,n);r->size+=n;r->data[r->size]=0;return n;
}
static struct json_object *post(const char *url,const char *token,struct json_object *request){
 CURL *c=curl_easy_init();struct curl_slist *headers=NULL;
 response_t *r=calloc(1,sizeof *r);struct json_object *result=NULL;
 char *authorization=NULL;size_t n;long code=0;
 if(!c||!r)goto done;
 n=strlen(token)+23;authorization=malloc(n);if(!authorization)goto done;
 snprintf(authorization,n,"Authorization: Bearer %s",token);
 headers=curl_slist_append(headers,authorization);
 headers=curl_slist_append(headers,"Content-Type: application/json");
 curl_easy_setopt(c,CURLOPT_URL,url);
 curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(c,CURLOPT_POSTFIELDS,request?json_object_to_json_string_ext(request,JSON_C_TO_STRING_PLAIN):"{}");
 curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,capture);
 curl_easy_setopt(c,CURLOPT_WRITEDATA,r);
 curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(c,CURLOPT_TIMEOUT,15L);
 if(curl_easy_perform(c)==CURLE_OK){
  curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);
  if(code==200)result=json_tokener_parse(r->data);
 }
done:
 if(authorization){erase(authorization,n);free(authorization);}
 curl_slist_free_all(headers);if(c)curl_easy_cleanup(c);
 if(r){erase(r,sizeof *r);free(r);}
 return result;
}
/* Transport-only Slack mrkdwn presentation; bounded and auditable.
 * Translate DSD's legacy escaped line endings, preserving the evidence text. */
int digit_slack_format_reply(const char *input,char *output,size_t capacity){
 size_t i=0,j=0;
 int dsd;
 const char *prefix="DSD SCOPED REPORT";
 if(!input||!output||capacity<2)return 0;
 dsd=!strncmp(input,prefix,strlen(prefix))&&
     (input[strlen(prefix)]==0||input[strlen(prefix)]=='\\'||input[strlen(prefix)]=='\n');
 if(dsd){
  if(capacity<3)return 0;
  output[j++]='*';
 }
 for(;input[i];i++){
  char ch=input[i];
  if(ch=='\\'&&input[i+1]=='n'){ch='\n';i++;}
  else if(ch=='\\'&&input[i+1]=='r'){
   ch='\n';i++;
   if(input[i+1]=='\\'&&input[i+2]=='n')i+=2;
  }else if(ch=='\r'){
   ch='\n';
   if(input[i+1]=='\n')i++;
  }
  if(j+1>=capacity)return 0;
  output[j++]=ch;
  if(dsd&&i==strlen(prefix)-1){
   if(j+1>=capacity)return 0;
   output[j++]='*';
  }
 }
 output[j]=0;
 return 1;
}
const char *digit_slack_reply_thread(const char *thread_ts){
 return thread_ts&&*thread_ts?thread_ts:NULL;
}
static int send_reply(const char *channel,const char *thread,const char *message){
 struct json_object *req=json_object_new_object(),*res;int ok;
 if(!req||!channel||!message){if(req)json_object_put(req);return 0;}
 json_object_object_add(req,"channel",json_object_new_string(channel));
 json_object_object_add(req,"text",json_object_new_string(message));
 if(thread&&*thread)json_object_object_add(req,"thread_ts",json_object_new_string(thread));
 res=post("https://slack.com/api/chat.postMessage",cfg.bot,req);
 ok=success(res);
 if(res)json_object_put(res);
 json_object_put(req);
 return ok;
}
/* Outbound intelligence only: fixed configured channel, never arbitrary destinations. */
static stnlabz_module_result_t publish_intel(const void *req,size_t len,void *out,
 size_t capacity,size_t *used,void *context){
 const digit_intel_delivery_t *msg=req;
 char formatted[DIGIT_INTEL_MESSAGE_MAX+256];
 int ok;
 (void)context;
 if(used)*used=0;
 if(!active||!msg||len!=sizeof *msg||msg->version!=DIGIT_INTEL_DELIVERY_VERSION||
    !memchr(msg->message,0,sizeof msg->message)||!msg->message[0]||
    !out||capacity<sizeof(int)||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!digit_slack_format_reply(msg->message,formatted,sizeof formatted))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 ok=send_reply(cfg.channel,NULL,formatted);
 memcpy(out,&ok,sizeof ok);*used=sizeof ok;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_START_FAILED;
}
static history_t *conversation(const char *user,const char *channel){
 time_t now=time(NULL);history_t *oldest=&history[0];size_t i;
 for(i=0;i<HISTORY_SIZE;i++){
  history_t *h=&history[i];
  if(h->seen&&now>=h->seen&&now-h->seen<1800&&
     !strcmp(h->user,user)&&!strcmp(h->channel,channel)){h->seen=now;return h;}
  if(!h->seen||h->seen<oldest->seen)oldest=h;
 }
 memset(oldest,0,sizeof *oldest);
 snprintf(oldest->user,sizeof oldest->user,"%s",user);
 snprintf(oldest->channel,sizeof oldest->channel,"%s",channel);
 oldest->seen=now;return oldest;
}
static int seen(const char *id){
 size_t i;
 if(!id||!*id||strlen(id)>=sizeof event_ids[0])return 1;
 for(i=0;i<128;i++)if(!strcmp(event_ids[i],id))return 1;
 snprintf(event_ids[event_cursor++%128],sizeof event_ids[0],"%s",id);
 return 0;
}
/* Different Slack event deliveries can represent the same source message. */
int digit_slack_seen_message(const char *workspace,const char *channel,const char *ts){
 char key[192];size_t i;int n;
 if(!workspace||!channel||!ts||!*workspace||!*channel||!*ts)return 1;
 n=snprintf(key,sizeof key,"%s/%s/%s",workspace,channel,ts);
 if(n<0||(size_t)n>=sizeof key)return 1;
 for(i=0;i<128;i++)if(!strcmp(message_keys[i],key))return 1;
 snprintf(message_keys[message_cursor++%128],sizeof message_keys[0],"%s",key);
 return 0;
}
static int diagnostic(const char *s){
 return !strcmp(s,"diagnose")||!strcmp(s,"diagnose my last request")||
        !strcmp(s,"diagnose last request");
}
/* Accept only explicit addressing in workspace channels.
 * A Slack mention, plain bot name, or bot name plus colon qualifies.
 * Arbitrary channel chatter is not routed into Ask. */
int digit_slack_addressed_text(const char *type,const char *channel,
 const char *bot,const char *raw,
 char *output,size_t capacity){
 const char *text=raw;
 char mention[96];size_t length;
 if(!type||!channel||!*channel||!bot||!raw||!output||capacity<2)return 0;
 if(strcmp(type,"message")&&strcmp(type,"app_mention"))return 0;
 snprintf(mention,sizeof mention,"<@%s>",bot);
 if(!strncmp(text,mention,strlen(mention)))text+=strlen(mention);
 else if(!strncasecmp(text,"digit-bot",9)&&
         (text[9]==':'||isspace((unsigned char)text[9])))
  text+=9;
 else if(!strncasecmp(text,"digit",5)&&
         (text[5]==':'||isspace((unsigned char)text[5])))text+=5;
 else return 0;
 while(*text&&(isspace((unsigned char)*text)||*text==':'||*text==','))text++;
 length=strlen(text);
 if(!length||length>=capacity||length>512||strchr(text,'\n')||strchr(text,'\r'))return 0;
 memcpy(output,text,length+1);
 return 1;
}
static void process(struct json_object *root){
 struct json_object *payload=obj(root,"payload"),*event=obj(payload,"event");
 const char *event_id=field(payload,"event_id"),*team=field(payload,"team_id");
 const char *type=field(event,"type"),*user=field(event,"user");
 const char *channel=field(event,"channel"),*raw=field(event,"text");
 const char *subtype=field(event,"subtype"),*bot=field(event,"bot_id");
 const char *ts=field(event,"thread_ts"),*message_ts=field(event,"ts");
 const char *kind=field(event,"channel_type"),*text=raw;
 int dm;char addressed[513]={0},answer[4096]={0};
 digit_router_request_t request={0};digit_ask_context_t ctx={0};
 history_t *h;size_t used=0;stnlabz_module_result_t status;
 if(!event||!type||!user||!channel||!raw||!team||
    strcmp(team,cfg.workspace)||subtype||bot||!strcmp(user,bot_id))return;
 dm=!strcmp(type,"message")&&kind&&!strcmp(kind,"im");
 if(!dm){
  if(!bot_id[0]||!digit_slack_addressed_text(type,channel,
                                                bot_id,raw,addressed,sizeof addressed))return;
  text=addressed;
 }
 while(*text&&isspace((unsigned char)*text))text++;
 if(!*text||strlen(text)>512||strchr(text,'\r')||strchr(text,'\n')||seen(event_id)||
    digit_slack_seen_message(team,channel,message_ts))return;
 h=conversation(user,channel);
 ctx.magic=DIGIT_ASK_CONTEXT_MAGIC;
 ctx.intel_report_authorized=cfg.operator[0]&&!strcmp(user,cfg.operator);
 snprintf(ctx.question,sizeof ctx.question,"%s",text);
 if(h->previous.request_id[0]){
  ctx.has_previous=1;
  memcpy(ctx.previous_request,h->previous.request_id,sizeof ctx.previous_request);
  memcpy(ctx.previous_route,h->previous.route,sizeof ctx.previous_route);
  memcpy(ctx.previous_service,h->previous.service,sizeof ctx.previous_service);
  ctx.previous_outcome=h->previous.outcome;
  ctx.previous_service_result=h->previous.service_result;
 }
 snprintf(request.request_id,sizeof request.request_id,"slack-%lu",++request_sequence);
 snprintf(request.path,sizeof request.path,"/ask");
 request.core_authorized=0;
 request.payload=&ctx;request.payload_size=sizeof ctx;
 status=host->invoke_service("router.dispatch",&request,sizeof request,answer,sizeof answer,&used);
 if(!diagnostic(text)){
  snprintf(h->previous.request_id,sizeof h->previous.request_id,"%s",request.request_id);
  snprintf(h->previous.route,sizeof h->previous.route,"%s",request.path);
  snprintf(h->previous.service,sizeof h->previous.service,"route./ask");
  h->previous.outcome=status==STNLABZ_MODULE_OK?0:5;
  h->previous.service_result=(int)status;
 }
 if(status!=STNLABZ_MODULE_OK||used<2||used>sizeof answer||!memchr(answer,0,used)){
  log_status("[SLACK] Ask dispatch unavailable");
  return;
 }
 {char formatted[8192];
  if(!digit_slack_format_reply(answer,formatted,sizeof formatted)){
   log_status("[SLACK] Reply presentation failed");return;
  }
  if(!send_reply(channel,digit_slack_reply_thread(ts),formatted))log_status("[SLACK] Reply delivery failed");
 }
}
static int acknowledge(CURL *ws,const char *envelope){
 struct json_object *j=json_object_new_object();const char *txt;
 CURLcode rc;size_t sent=0,len;int ok;
 if(!j)return 0;
 json_object_object_add(j,"envelope_id",json_object_new_string(envelope));
 txt=json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN);
 len=strlen(txt);
 rc=curl_ws_send(ws,txt,len,&sent,0,CURLWS_TEXT);
 ok=rc==CURLE_OK&&sent==len;
 json_object_put(j);return ok;
}
static void socket_session(const char *url){
 CURL *ws=curl_easy_init();char buf[RESPONSE_MAX];size_t count=0;
 if(!ws)return;
 curl_easy_setopt(ws,CURLOPT_URL,url);
 curl_easy_setopt(ws,CURLOPT_CONNECT_ONLY,2L);
 curl_easy_setopt(ws,CURLOPT_CONNECTTIMEOUT,8L);
 if(curl_easy_perform(ws)!=CURLE_OK){curl_easy_cleanup(ws);log_status("[SLACK] Socket connection failed");return;}
 while(!atomic_load(&stopping)){
  const struct curl_ws_frame *frame=NULL;size_t received=0;CURLcode rc;
  rc=curl_ws_recv(ws,buf+count,sizeof buf-count-1,&received,&frame);
  if(rc==CURLE_AGAIN){struct timespec pause={0,100000000};nanosleep(&pause,NULL);continue;}
  if(rc!=CURLE_OK||!frame||(frame->flags&CURLWS_CLOSE))break;
  count+=received;
  if(count>=sizeof buf-1)break;
  if(frame->bytesleft)continue;
  buf[count]=0;count=0;
  {struct json_object *root=json_tokener_parse(buf),*payload;
   const char *envelope=field(root,"envelope_id");
   if(root&&envelope&&acknowledge(ws,envelope)){
    payload=obj(root,"payload");(void)payload;
    process(root);
   }
   if(root)json_object_put(root);
  }
 }
 curl_easy_cleanup(ws);
}
static void *run(void *unused){
 (void)unused;
 while(!atomic_load(&stopping)){
  struct json_object *res,*req=json_object_new_object();
  const char *url=NULL;char urlcopy[2048]={0};
  if(!req)break;
  res=post("https://slack.com/api/apps.connections.open",cfg.app,req);
  json_object_put(req);
  if(res&&success(res))url=field(res,"url");
  if(url&&!strncmp(url,"wss://",6))snprintf(urlcopy,sizeof urlcopy,"%s",url);
  if(res)json_object_put(res);
  if(urlcopy[0])socket_session(urlcopy);
  else log_status("[SLACK] Socket Mode session unavailable");
  erase(urlcopy,sizeof urlcopy);
  {unsigned i;for(i=0;i<10&&!atomic_load(&stopping);i++)sleep(1);}
 }
 return NULL;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 struct json_object *res,*req;const char *id=NULL;
 if(active||!h||!h->invoke_service||!h->send_message||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!configuration())return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;
 req=json_object_new_object();
 if(!req){host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
 res=post("https://slack.com/api/auth.test",cfg.bot,req);
 json_object_put(req);
 if(res&&success(res))id=field(res,"user_id");
 if(id)snprintf(bot_id,sizeof bot_id,"%s",id);
 if(res)json_object_put(res);
 if(!bot_id[0]){log_status("[SLACK] Bot identity verification failed");erase(&cfg,sizeof cfg);host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
 memset(history,0,sizeof history);memset(event_ids,0,sizeof event_ids);
 memset(message_keys,0,sizeof message_keys);message_cursor=0;
 request_sequence=0;event_cursor=0;
 atomic_store(&stopping,0);
 if(pthread_create(&worker,NULL,run,NULL)){erase(&cfg,sizeof cfg);host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
 active=1;
 if(!host->register_service(DIGIT_INTEL_SLACK_SERVICE,publish_intel,NULL)){
  atomic_store(&stopping,1);pthread_join(worker,NULL);active=0;
  erase(&cfg,sizeof cfg);host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 log_status("[SLACK] Transport started; Socket Mode connecting");
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(host&&host->unregister_service)(void)host->unregister_service(DIGIT_INTEL_SLACK_SERVICE,NULL);
 atomic_store(&stopping,1);
 pthread_join(worker,NULL);
 active=0;erase(&cfg,sizeof cfg);erase(bot_id,sizeof bot_id);host=NULL;
 return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"slack");
 snprintf(d.name,sizeof d.name,"Digit Slack Transport");
 d.version_major=0;d.version_minor=1;d.version_patch=6;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
