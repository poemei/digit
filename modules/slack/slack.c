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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"

#define CONFIG_PATH "/opt/digit/auth/slack/slack.conf"
#define CONVERSATION_SERVICE "digit.conversation"
#define INTEL_DELIVERY "digit.slack.intel.deliver"
#define MODULE_DELIVERY "digit.slack.modules.deliver"
#define MAX_REPORT 16000
#define WS_BUFFER 65536

typedef struct {
    char sender[128], account[128], target[128], message[512];
} digit_message_t; /* Matches current Conversation request ABI. */
typedef struct {
    int enabled;
    char workspace[64], app_token[512], bot_token[512];
    char intel_channel[64], module_channel[64], learn_user[64];
    char learn_key[256]; /* Reserved for unimplemented Lessons; never consumed. */
} slack_config_t;
typedef struct {char *bytes;size_t used,cap;} response_buffer_t;
static slack_config_t config;
static const stnlabz_module_host_t *host_api;
static pthread_t worker;
static atomic_int running=0;
static int worker_started;
static pthread_mutex_t post_lock=PTHREAD_MUTEX_INITIALIZER;

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
static int ws_send(CURL *curl,const char *text){size_t sent=0;size_t len=strlen(text);
 return curl_ws_send(curl,text,len,&sent,0,CURLWS_TEXT)==CURLE_OK&&sent==len;
}
static void on_event(struct json_object *root){
 struct json_object *payload=obj(root,"payload"),*event=obj(payload,"event");
 const char *type=field(event,"type"),*team=field(payload,"team_id");
 const char *user=field(event,"user"),*channel=field(event,"channel");
 const char *text=field(event,"text"),*thread=field(event,"thread_ts");
 const char *subtype=field(event,"subtype"),*event_id=field(payload,"event_id");
 digit_message_t msg={0};char answer[8192]={0};size_t used=0;
 int direct;
 (void)event_id;
 if(!event||!team||strcmp(team,config.workspace)||!type||!user||!channel||
    !text||subtype||!host_api||!host_api->invoke_service)return;
 direct=!strcmp(type,"message")&&field(event,"channel_type")&&
  !strcmp(field(event,"channel_type"),"im");
 if(!direct&&strcmp(type,"app_mention"))return;
 if(!copy(msg.sender,sizeof msg.sender,user)||
    !copy(msg.account,sizeof msg.account,user)||
    !copy(msg.target,sizeof msg.target,channel))return;
 /* Mentions retain the original user text; Conversation performs interpretation. */
 if(!copy(msg.message,sizeof msg.message,text))return;
 if(host_api->invoke_service(CONVERSATION_SERVICE,&msg,sizeof msg,
      answer,sizeof answer,&used)!=STNLABZ_MODULE_OK)return;
 if(used&&used<sizeof answer){answer[used]=0;
  (void)posted(channel,answer,thread);
 }
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
   if(rc==CURLE_AGAIN){struct timespec nap={0,100000000L};nanosleep(&nap,NULL);continue;}
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
 slack_config_t saved=config;char x[8];char good[]={"hello"};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 check(r,copy(x,sizeof x,"abc"),0);
 check(r,!copy(x,sizeof x,"abcdefgh"),1);
 check(r,!copy(x,sizeof x,"abcdefghijk"),1);
 check(r,clean_report(good,sizeof good),0);
 check(r,!clean_report(NULL,0),1);
 {char e[]={""};check(r,!clean_report(e,sizeof e),1);}
 {char c[]={"x\r"};check(r,!clean_report(c,sizeof c),1);}
 {char c[]={"x\ny"};check(r,clean_report(c,sizeof c),0);}
 check(r,!private_config("/path/not/present"),1);
 check(r,!strcmp(CONVERSATION_SERVICE,"digit.conversation"),0);
 check(r,strcmp(INTEL_DELIVERY,MODULE_DELIVERY)!=0,0);
 check(r,MAX_REPORT>=8192,0);
 config=saved;
 return r->tests_failed?STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
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
 atomic_store(&running,1);
 if(pthread_create(&worker,NULL,socket_worker,NULL)!=0){
  atomic_store(&running,0);
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
 if(!host_api->unregister_service(MODULE_DELIVERY,config.module_channel)||
    !host_api->unregister_service(INTEL_DELIVERY,config.intel_channel))
    return STNLABZ_MODULE_ERR_STOP_FAILED;
 host_api=NULL;wipe(&config,sizeof config);return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="slack",.name="Slack ABI Transport",
 .version_major=0,.version_minor=1,.version_patch=1,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
/* [End AI:GPT-6] */
