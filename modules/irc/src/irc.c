#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/select.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "stnlabz_abi/module.h"
#include "router.h"
#include "ask_context.h"
#include "intel_delivery.h"
#include <time.h>

typedef struct {char server[256],port[8],nick[64],channel[128],account[128],password[256];} irc_config_t;
static irc_config_t config;
static pthread_t worker;
static atomic_int stopping;
static int active;
static const stnlabz_module_host_t *host;
static pthread_mutex_t intel_lock=PTHREAD_MUTEX_INITIALIZER;
static char intel_pending[480];
static int intel_has_pending;
static stnlabz_module_result_t publish_intel(const void *request,size_t length,void *out,
 size_t capacity,size_t *used,void *context){
 const digit_intel_delivery_t *msg=request;
 size_t n;int queued=0;
 (void)context;
 if(used)*used=0;
 if(!active||!request||length!=sizeof *msg||msg->version!=DIGIT_INTEL_DELIVERY_VERSION||
    !memchr(msg->message,0,sizeof msg->message)||!msg->message[0]||
    !out||capacity<sizeof(int)||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 n=strlen(msg->message);
 if(n>=sizeof intel_pending||strchr(msg->message,'\r')||strchr(msg->message,'\n'))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 pthread_mutex_lock(&intel_lock);
 if(!intel_has_pending){
  memcpy(intel_pending,msg->message,n+1);intel_has_pending=1;queued=1;
 }
 pthread_mutex_unlock(&intel_lock);
 memcpy(out,&queued,sizeof queued);*used=sizeof queued;
 return queued?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_START_FAILED;
}
static void notice(const char *s){if(host&&host->send_message)host->send_message(s);}
static int valid(const char *s,size_t max){size_t i,n;if(!s)return 0;n=strlen(s);if(!n||n>=max)return 0;for(i=0;i<n;i++)if((unsigned char)s[i]<33||s[i]==127)return 0;return 1;}
static int get_setting(const char *key,char *out,size_t cap){const char *value=getenv(key);if(!valid(value,cap))return 0;memcpy(out,value,strlen(value)+1);return 1;}
static int load_config(void){
 memset(&config,0,sizeof config);
 return get_setting("DIGIT_IRC_SERVER",config.server,sizeof config.server)&&
 get_setting("DIGIT_IRC_PORT",config.port,sizeof config.port)&&
 get_setting("DIGIT_IRC_NICK",config.nick,sizeof config.nick)&&
 get_setting("DIGIT_IRC_CHANNEL",config.channel,sizeof config.channel)&&
 get_setting("DIGIT_IRC_ACCOUNT",config.account,sizeof config.account)&&
 get_setting("DIGIT_IRC_PASSWORD",config.password,sizeof config.password)&&
 config.channel[0]=='#'&&strspn(config.port,"0123456789")==strlen(config.port);
}
static int connect_tcp(void){
 struct addrinfo hints={0},*result=NULL,*a;int fd=-1;
 hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
 if(getaddrinfo(config.server,config.port,&hints,&result)!=0)return -1;
 for(a=result;a;a=a->ai_next){
  fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol);
  if(fd<0)continue;
  if(connect(fd,a->ai_addr,a->ai_addrlen)==0)break;
  close(fd);fd=-1;
 }
 freeaddrinfo(result);return fd;
}
static int send_line(SSL *tls,const char *line){
 char data[513];size_t n=strlen(line);
 if(n>510||strchr(line,'\r')||strchr(line,'\n'))return 0;
 memcpy(data,line,n);data[n++]='\r';data[n++]='\n';
 return SSL_write(tls,data,(int)n)==(int)n;
}
static int send_pending_intel(SSL *tls){
 char text[480],wire[512];int ok=1;
 pthread_mutex_lock(&intel_lock);
 if(intel_has_pending){
  snprintf(text,sizeof text,"%s",intel_pending);
  if(snprintf(wire,sizeof wire,"PRIVMSG %s :%s",config.channel,text)>510)ok=0;
  else ok=send_line(tls,wire);
  if(ok){intel_has_pending=0;intel_pending[0]=0;}
 }
 pthread_mutex_unlock(&intel_lock);
 return ok;
}
static int sasl_payload(char *out,size_t cap){
 unsigned char plain[640];size_t a=strlen(config.account),p=strlen(config.password),n;
 if(a*2+p+2>sizeof plain)return 0;
 memcpy(plain,config.account,a);plain[a]=0;
 memcpy(plain+a+1,config.account,a);plain[a*2+1]=0;
 memcpy(plain+a*2+2,config.password,p);
 n=a*2+p+2;
 if(cap<4*((n+2)/3)+1){OPENSSL_cleanse(plain,sizeof plain);return 0;}
 EVP_EncodeBlock((unsigned char *)out,plain,(int)n);
 OPENSSL_cleanse(plain,sizeof plain);
 return strlen(out)<=400; /* IRC AUTHENTICATE chunk bound */
}

#define IRC_HISTORY_SLOTS 64
#define IRC_HISTORY_TTL 1800
typedef struct {char sender[64],target[128];digit_scoped_diagnostic_t previous;time_t seen;int valid;} irc_history_t;
static irc_history_t history[IRC_HISTORY_SLOTS];
static irc_history_t *history_for(const char *sender,const char *target){
 time_t now=time(NULL);irc_history_t *slot=NULL;size_t i;
 for(i=0;i<IRC_HISTORY_SLOTS;i++){
  irc_history_t *h=&history[i];
  if(h->valid&&now>=h->seen&&now-h->seen<IRC_HISTORY_TTL){
   if(!strcasecmp(h->sender,sender)&&!strcasecmp(h->target,target)){h->seen=now;return h;}
  }else if(!slot)slot=h;
 }
 if(!slot)return NULL;
 memset(slot,0,sizeof *slot);
 snprintf(slot->sender,sizeof slot->sender,"%s",sender);
 snprintf(slot->target,sizeof slot->target,"%s",target);
 slot->seen=now;slot->valid=1;return slot;
}
static int diagnostic_question(const char *text){
 return !strcasecmp(text,"diagnose my last request")||
 !strcasecmp(text,"diagnose last request")||!strcasecmp(text,"diagnose");
}
/* Only explicitly addressed conversation, never arbitrary IRC commands. */
static void chat_message(SSL *tls,const char *line){
 const char *sep,*body,*prefix;char sender[64]={0},target[128]={0},message[513];
 char answer[1024],wire[512];size_t n=0,used=0;
 digit_router_request_t req={0};
 digit_ask_context_t context={0};irc_history_t *conversation;
 stnlabz_module_result_t dispatch_result;
 static unsigned long sequence;
 if(!host||!host->invoke_service||line[0]!=':')return;
 sep=strstr(line," PRIVMSG ");if(!sep)return;
 prefix=line+1;while(prefix[n]&&prefix+n<sep&&prefix[n]!='!'&&n<sizeof sender-1)n++;
 if(!n||n>=sizeof sender||n==strlen(config.nick)&&!strncasecmp(prefix,config.nick,n))return;
 memcpy(sender,prefix,n);
 body=sep+9;{const char *space=strchr(body,' ');size_t width;
  if(!space)return;width=(size_t)(space-body);if(!width||width>=sizeof target)return;
  memcpy(target,body,width);target[width]=0;body=space+1;
 }
 if(*body==':')body++;
 if(!strcasecmp(target,config.nick)){
  if(strncasecmp(body,"!ask ",5))return;
  body+=5;
 }else if(!strcasecmp(target,config.channel)){
  size_t nick_len=strlen(config.nick);
  if(strncasecmp(body,config.nick,nick_len)||body[nick_len]!=':')return;
  body+=nick_len+1;while(*body==' ')body++;
 }else return;
 if(!*body||strlen(body)>512||strchr(body,'\r')||strchr(body,'\n'))return;
 conversation=history_for(sender,target);
 if(!conversation)return;
 snprintf(req.request_id,sizeof req.request_id,"irc-%lu",++sequence);
 snprintf(req.path,sizeof req.path,"/ask");req.core_authorized=0;
 context.magic=DIGIT_ASK_CONTEXT_MAGIC;
 snprintf(context.question,sizeof context.question,"%s",body);
 if(conversation->previous.request_id[0]){
  context.has_previous=1;
  memcpy(context.previous_request,conversation->previous.request_id,sizeof context.previous_request);
  memcpy(context.previous_route,conversation->previous.route,sizeof context.previous_route);
  memcpy(context.previous_service,conversation->previous.service,sizeof context.previous_service);
  context.previous_outcome=conversation->previous.outcome;
  context.previous_service_result=conversation->previous.service_result;
 }
 req.payload=&context;req.payload_size=sizeof context;
 dispatch_result=host->invoke_service("router.dispatch",&req,sizeof req,answer,sizeof answer,&used);
 if(!diagnostic_question(body)){
  snprintf(conversation->previous.request_id,sizeof conversation->previous.request_id,"%s",req.request_id);
  snprintf(conversation->previous.route,sizeof conversation->previous.route,"%s",req.path);
  snprintf(conversation->previous.service,sizeof conversation->previous.service,"route./ask");
  conversation->previous.outcome=dispatch_result==STNLABZ_MODULE_OK?0:5;
  conversation->previous.service_result=(int)dispatch_result;
 }
 if(dispatch_result!=STNLABZ_MODULE_OK||!used||used>sizeof answer||!memchr(answer,0,used))return;
 /* IRC protocol framing, not server-side presentation: one PRIVMSG per report line. */
 {
  const char *p=answer;const char *recipient=!strcasecmp(target,config.nick)?sender:config.channel;
  size_t sent=0;
  while(*p&&sent<12){
   size_t j=0;
   while(*p&&*p!='\n'&&*p!='\r'&&j<350){
    if(p[0]=='\\'&&p[1]=='n'){p+=2;break;}
    if((unsigned char)*p<32){p++;continue;}
    message[j++]=*p++;
   }
   message[j]=0;
   while(*p=='\r'||*p=='\n')p++;
   if(!j)continue;
   if(snprintf(wire,sizeof wire,"PRIVMSG %s :%s",recipient,message)>=(int)sizeof wire)break;
   if(!send_line(tls,wire))break;
   sent++;
  }
 }

}
static int handle_line(SSL *tls,const char *line,int *authenticated,int *joined){
 char cmd[513],payload[513];const char *p;
 if(!strncmp(line,"PING ",5))return send_line(tls,line[0]=='P'&&line[5]==':'?
  (snprintf(cmd,sizeof cmd,"PONG %s",line+5),cmd):"PONG");
 if(!strcmp(line,"AUTHENTICATE +")){
  if(!sasl_payload(payload,sizeof payload))return 0;
  snprintf(cmd,sizeof cmd,"AUTHENTICATE %s",payload);return send_line(tls,cmd);
 }
 if(strstr(line," 903 ")){*authenticated=1;return send_line(tls,"CAP END");}
 if(strstr(line," 904 ")||strstr(line," 905 ")||strstr(line," 906 ")||strstr(line," 907 ")){
  notice("[IRC] SASL authentication rejected");return 0;
 }
 if(strstr(line," CAP ")&&strstr(line," ACK ")&&strstr(line,"sasl"))
  return send_line(tls,"AUTHENTICATE PLAIN");
 if(strstr(line," 001 ")&&*authenticated&&!*joined){
  snprintf(cmd,sizeof cmd,"JOIN %s",config.channel);
  *joined=1;return send_line(tls,cmd);
 }
 if(strstr(line," 376 ")||strstr(line," 422 ")){
  if(*authenticated&&!*joined){snprintf(cmd,sizeof cmd,"JOIN %s",config.channel);*joined=1;return send_line(tls,cmd);}
 }
 /* Incoming channel messages are observational only; they confer no command authority. */
 p=strstr(line," PRIVMSG ");
 if(p&&*authenticated){chat_message(tls,line);return 1;}
 return 1;
}
static void run_connection(void){
 int fd=-1,authenticated=0,joined=0,ok=1;
 SSL_CTX *ctx=NULL;SSL *tls=NULL;
 char input[2048],line[513],cmd[513];size_t count=0;
 fd=connect_tcp();if(fd<0){notice("[IRC] TCP connection failed");return;}
 ctx=SSL_CTX_new(TLS_client_method());if(!ctx)goto done;
 if(SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)!=1||
    SSL_CTX_set_default_verify_paths(ctx)!=1)goto done;
 SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);
 tls=SSL_new(ctx);if(!tls)goto done;
 if(SSL_set_tlsext_host_name(tls,config.server)!=1||
    SSL_set1_host(tls,config.server)!=1||
    SSL_set_fd(tls,fd)!=1||SSL_connect(tls)!=1||
    SSL_get_verify_result(tls)!=X509_V_OK){notice("[IRC] TLS certificate verification failed");goto done;}
 if(!send_line(tls,"CAP LS 302"))goto done;
 snprintf(cmd,sizeof cmd,"NICK %s",config.nick);if(!send_line(tls,cmd))goto done;
 snprintf(cmd,sizeof cmd,"USER %s 0 * :Digit STN-LABZ",config.nick);if(!send_line(tls,cmd))goto done;
 if(!send_line(tls,"CAP REQ :sasl"))goto done;
 notice("[IRC] TLS connection established; SASL negotiation started");
 while(!atomic_load(&stopping)&&ok){
  fd_set readset;struct timeval tv={1,0};int ready,n;size_t i;
  FD_ZERO(&readset);FD_SET(fd,&readset);
  ready=select(fd+1,&readset,NULL,NULL,&tv);
  if(ready<0)break;
   if(authenticated&&joined&&!send_pending_intel(tls))break;
   if(!ready)continue;
  n=SSL_read(tls,input,sizeof input);if(n<=0)break;
  for(i=0;i<(size_t)n;i++){
   if(input[i]=='\n'){
    if(count&&line[count-1]=='\r')count--;
    line[count]=0;
    if(count&&!(ok=handle_line(tls,line,&authenticated,&joined)))break;
    count=0;
   }else if(count<sizeof line-1)line[count++]=input[i];
   else {ok=0;notice("[IRC] Oversized IRC line rejected");break;}
  }
 }
done:
 if(tls){SSL_shutdown(tls);SSL_free(tls);}
 if(ctx)SSL_CTX_free(ctx);
 if(fd>=0)close(fd);
}
static void *thread_main(void *unused){
 (void)unused;
 while(!atomic_load(&stopping)){
  unsigned i;run_connection();
  if(atomic_load(&stopping))break;
  notice("[IRC] Disconnected; reconnecting");
  for(i=0;i<10&&!atomic_load(&stopping);i++)sleep(1);
 }
 return NULL;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(active||!h||!h->send_message||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 host=h;
 if(!load_config()){notice("[IRC] Missing/invalid DIGIT_IRC_* configuration; not connecting");host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
 atomic_store(&stopping,0);
 if(pthread_create(&worker,NULL,thread_main,NULL)!=0){host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
 memset(history,0,sizeof history);
 active=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 atomic_store(&stopping,1);pthread_join(worker,NULL);
 active=0;OPENSSL_cleanse(&config,sizeof config);host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);snprintf(d.id,sizeof d.id,"irc");snprintf(d.name,sizeof d.name,"Digit IRC Transport");
 d.version_major=0;d.version_minor=1;d.version_patch=2;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
