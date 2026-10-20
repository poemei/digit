/* [AI:GPT-6 | 2026-10-19 00:30:11 UTC]
 * Digit IRC transport module for Libera.Chat, ABI 1.5.
 * Core interprets input through an ABI service; this module owns only IRC I/O.
 * Qualification deliberately fails until the mandatory 20 transport tests exist.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "module.h"
#include "socials.h"
#include "dsd.h"
#include "../observation/observation.h"
#include <time.h>
#include <syslog.h>

#define IRC_ENV_PATH "/opt/digit/state/auth/irc.env"
#define IRC_INPUT_SERVICE "digit.message"
#define IRC_CONVERSATION_LISTEN "digit.conversation.listen"
typedef struct {
    char server[256], port[16], nick[64], channel[128];
    char account[128], password[256];
    char mommy_account[128], auntie_account[128], sister_account[128];
} irc_config_t;
typedef struct {
    char sender[128], account[128], target[128], message[512];
} irc_input_t;
static pthread_t worker;
static int active_socket=-1;
static pthread_mutex_t socket_mutex=PTHREAD_MUTEX_INITIALIZER;
#define S2_DELIVER_SERVICE "digit.irc.deliver"
#define S2_REPORT_BYTES 384
#define S2_QUEUE_CAP 16
typedef struct {char report[S2_REPORT_BYTES];} irc_s2_report_t;
static irc_s2_report_t outbound[S2_QUEUE_CAP];
static unsigned outbound_head,outbound_count;
static pthread_mutex_t outbound_mutex=PTHREAD_MUTEX_INITIALIZER;
static int wake_pipe[2]={-1,-1};
static int irc_ready;
static int safe_report(const char *p,size_t cap) {
    size_t i;
    if(!p||!memchr(p,0,cap))return 0;
    for(i=0;p[i];i++)if(p[i]=='\r'||p[i]=='\n'||(unsigned char)p[i]<32)return 0;
    return i>0&&i<=350;
}
static stnlabz_module_result_t deliver_service(
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *used,void *context) {
    const irc_s2_report_t *r=request;
    unsigned index;unsigned char signal_byte=1;
    (void)response;(void)response_size;(void)context;
    if(used)*used=0;
    if(!r||request_size!=sizeof *r||!safe_report(r->report,sizeof r->report))
      return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    pthread_mutex_lock(&outbound_mutex);
    if(!irc_ready||outbound_count==S2_QUEUE_CAP){
      pthread_mutex_unlock(&outbound_mutex);return STNLABZ_MODULE_ERR_INVALID_STATE;
    }
    index=(outbound_head+outbound_count)%S2_QUEUE_CAP;
    outbound[index]=*r;outbound_count++;
    pthread_mutex_unlock(&outbound_mutex);
    if(wake_pipe[1]>=0){
        ssize_t signaled=write(wake_pipe[1],&signal_byte,1);
        if(signaled<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)
            syslog(LOG_WARNING,"digit.irc wake notification failed: %d",errno);
    }
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_host_t *host_api;
/* DM-only DSD bridge; trace ownership never keyed by unverified nick. */
#define DSD_DM_SLOTS 32
typedef struct {char account[128];uint64_t trace;char origin[256];} dsd_dm_t;
static dsd_dm_t dsd_dm[DSD_DM_SLOTS];
static uint64_t dsd_next_trace=1;
static unsigned dsd_dm_evict;
static uint64_t dsd_previous(const char *account){
 unsigned i;
 for(i=0;i<DSD_DM_SLOTS;i++)
  if(!strcmp(dsd_dm[i].account,account))return dsd_dm[i].trace;
 return 0;
}
static void dsd_remember(const char *account,uint64_t trace){
 unsigned i,empty=DSD_DM_SLOTS;
 if(!account||!account[0]||!trace)return;
 for(i=0;i<DSD_DM_SLOTS;i++){
  if(!strcmp(dsd_dm[i].account,account)){dsd_dm[i].trace=trace;return;}
  if(!dsd_dm[i].account[0]&&empty==DSD_DM_SLOTS)empty=i;
 }
 if(empty==DSD_DM_SLOTS)empty=dsd_dm_evict++%DSD_DM_SLOTS;
 memset(&dsd_dm[empty],0,sizeof dsd_dm[empty]);
 snprintf(dsd_dm[empty].account,sizeof dsd_dm[empty].account,"%s",account);
 dsd_dm[empty].trace=trace;
}
static void dsd_remember_origin(const char *account,uint64_t trace,const char *message){
 unsigned i,j=0;
 if(!account||!trace||!message)return;
 for(i=0;i<DSD_DM_SLOTS;i++){
  if(dsd_dm[i].trace!=trace||strcmp(dsd_dm[i].account,account))continue;
  memset(dsd_dm[i].origin,0,sizeof dsd_dm[i].origin);
  while(*message&&j<sizeof dsd_dm[i].origin-1){
   unsigned char c=(unsigned char)*message++;
   if(c<32||c==127)continue;
   dsd_dm[i].origin[j++]=(char)c;
  }
  return;
 }
}
static const char *dsd_origin(const char *account,uint64_t trace){
 unsigned i;
 for(i=0;i<DSD_DM_SLOTS;i++)
  if(dsd_dm[i].trace==trace&&!strcmp(dsd_dm[i].account,account))
   return dsd_dm[i].origin;
 return "";
}
static int dsd_command(const char *message){
 return !strcmp(message,"show dsd")||
  !strcmp(message,"show trace")||
  !strcmp(message,"diagnose my last request")||
  !strncmp(message,"diagnose trace ",15)||
  !strncmp(message,"diagnose module ",16);
}
static void dsd_record_message(const char *account,
 uint64_t trace,int replied,stnlabz_module_result_t service_result,unsigned line){
 digit_dsd_record_t record={0};
 size_t used=0;
 if(!account||!account[0]||!host_api||!host_api->invoke_service)return;
 record.trace_id=record.event.trace_id=trace;
 record.event.span_id=1;
 snprintf(record.event.module,sizeof record.event.module,"irc");
 snprintf(record.event.method,sizeof record.event.method,"incoming");
 snprintf(record.event.file,sizeof record.event.file,"modules/irc/irc.c");
 snprintf(record.event.operation,sizeof record.event.operation,"digit.message");
 record.event.line=line;
 record.event.result=service_result==STNLABZ_MODULE_OK?0:1;
 /* Record the actual ABI disposition. No reply is not an ABI failure. A non-OK return is not necessarily
  * an unavailable service, and a successful empty return is not an error
  * from IRC transport. */
 const char *disposition=service_result==STNLABZ_MODULE_OK?
     (replied?"RESPONSE_RETURNED":"NO_RESPONSE"):
     service_result==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":
     service_result==STNLABZ_MODULE_ERR_NOT_AUTHORIZED?"NOT_AUTHORIZED":
     service_result==STNLABZ_MODULE_ERR_INVALID_ARGUMENT?"INVALID_ARGUMENT":
     service_result==STNLABZ_MODULE_ERR_INVALID_STATE?"INVALID_STATE":
     service_result==STNLABZ_MODULE_ERR_QUARANTINED?"QUARANTINED":
     service_result==STNLABZ_MODULE_ERR_NOT_QUALIFIED?"NOT_QUALIFIED":
     "SERVICE_ERROR";
 snprintf(record.event.status,sizeof record.event.status,"%s",disposition);
 if(host_api->invoke_service(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,
   NULL,0,&used)==STNLABZ_MODULE_OK)dsd_remember(account,trace);
}
/* Capture "show dsd" as the prior DM request after its service returns.
 * A diagnosis itself must never replace the request it investigates. */
static int dsd_tracks_previous(const char *message){
 return message&&!strcmp(message,"show dsd");
}
static void dsd_record_command(const char *account,uint64_t trace,
 const char *status,unsigned line){
 digit_dsd_record_t record={0};
 size_t used=0;
 if(!account||!account[0]||!trace||!host_api||!host_api->invoke_service)return;
 record.trace_id=record.event.trace_id=trace;
 record.event.span_id=1;
 record.event.line=line;
 record.event.result=!strcmp(status,"RESPONSE_RETURNED")?0:1;
 snprintf(record.event.module,sizeof record.event.module,"dsd");
 snprintf(record.event.method,sizeof record.event.method,"command");
 snprintf(record.event.file,sizeof record.event.file,"modules/irc/irc.c");
 snprintf(record.event.operation,sizeof record.event.operation,"digit.dsd.command");
 snprintf(record.event.status,sizeof record.event.status,"%s",status);
 if(host_api->invoke_service(DIGIT_DSD_RECORD_SERVICE,&record,sizeof record,
    NULL,0,&used)==STNLABZ_MODULE_OK)dsd_remember(account,trace);
}
static int dsd_dm_reply(const char *account,const char *message,char *response,
 size_t cap,size_t *used){
 digit_dsd_command_request_t query={0};
 digit_dsd_command_result_t answer={0};
 size_t returned=0;
 stnlabz_module_result_t rc;
 if(!account||!account[0]||!message||!response||!used||!host_api)return 0;
 *used=0;
 query.previous_trace_id=dsd_previous(account);
 /* Trace IDs and module-wide history cannot bypass DM ownership. */
 if(strcmp(message,"show dsd")&&strcmp(message,"show trace")&&
    strcmp(message,"diagnose my last request")){
  snprintf(response,cap,"DSD: Use diagnose my last request. Other trace investigations require scoped authorization.");
  *used=strlen(response);return 1;
 }
 snprintf(query.command,sizeof query.command,"%s",message);
 rc=host_api->invoke_service(DIGIT_DSD_COMMAND_SERVICE,&query,sizeof query,
   &answer,sizeof answer,&returned);
 if(rc!=STNLABZ_MODULE_OK||returned!=sizeof answer||
    !memchr(answer.text,0,sizeof answer.text)) {
  snprintf(response,cap,"DSD is unavailable or not active.");
 }else snprintf(response,cap,"%s",answer.text);
 *used=strlen(response);
 return 1;
}

static int worker_started;
static volatile sig_atomic_t stopping;
static time_t last_greeting;
#define IRC_GREETING_INTERVAL 30

static int assign(char *out,size_t cap,const char *in) {
    size_t n = strlen(in);
    if(n>=2 && ((in[0]=='"'&&in[n-1]=='"')||(in[0]=='\''&&in[n-1]=='\''))) {
        in++;n-=2;
    }
    if(!n||n>=cap)return 0;
    memcpy(out,in,n);out[n]=0;return 1;
}
static int config_valid(const irc_config_t *c) {
    return c && !strcmp(c->server,"irc.libera.chat") &&
        !strcmp(c->port,"6697") && !strcmp(c->channel,"#stn-labz") &&
        c->nick[0] && c->account[0] && c->password[0] &&
        !strpbrk(c->nick," \r\n") && !strpbrk(c->account,"\r\n");
}
static int reply_valid(const char *reply,size_t size) {
    return reply && size>0 && size<512 && !memchr(reply,'\r',size) &&
        !memchr(reply,'\n',size) && !memchr(reply,'\0',size);
}
/* Address parsing is transport normalization, not a participation decision. */
static size_t address_prefix(const char *body,const char *nick){
 const char *names[3]={nick,"digit","digicat"};unsigned i;
 if(!body)return 0;
 while(isspace((unsigned char)*body))body++;
 for(i=0;i<3;i++){
  size_t n;
  if(!names[i]||!names[i][0])continue;
  n=strlen(names[i]);
  if(strncasecmp(body,names[i],n))continue;
  if(body[n]==':'||body[n]==','||body[n]==' '||
     body[n]=='?'||body[n]=='!'||body[n]==0)return n;
 }
 return 0;
}
/* Only address-normalized, non-inquiry emoticon contact.
 * This is transport recognition; social expression belongs to Socials. */
static int social_emoticon(const char *p){
 static const char *const forms[]={":)",":D",";)",":3","<3","^_^",":xD","=^.^="};
 size_t i;
 if(!p)return 0;
 for(i=0;i<sizeof forms/sizeof forms[0];i++)
  if(!strcmp(p,forms[i]))return 1;
 return 0;
}
static int config_read(irc_config_t *c) {
    FILE *f;char line[1024];
    memset(c,0,sizeof *c);
    f=fopen(IRC_ENV_PATH,"r");if(!f)return 0;
    while(fgets(line,sizeof line,f)) {
        char *p=line,*sep,*end;
        while(isspace((unsigned char)*p))p++;
        if(!strncmp(p,"export ",7))p+=7;
        if(*p=='#'||!*p)continue;
        sep=strchr(p,'=');if(!sep)continue;*sep++=0;
        end=p+strlen(p);while(end>p&&isspace((unsigned char)end[-1]))*--end=0;
        end=sep+strlen(sep);while(end>sep&&isspace((unsigned char)end[-1]))*--end=0;
        if(!strcmp(p,"DIGIT_IRC_SERVER"))(void)assign(c->server,sizeof c->server,sep);
        else if(!strcmp(p,"DIGIT_IRC_PORT"))(void)assign(c->port,sizeof c->port,sep);
        else if(!strcmp(p,"DIGIT_IRC_NICK"))(void)assign(c->nick,sizeof c->nick,sep);
        else if(!strcmp(p,"DIGIT_IRC_CHANNEL"))(void)assign(c->channel,sizeof c->channel,sep);
        else if(!strcmp(p,"DIGIT_IRC_ACCOUNT"))(void)assign(c->account,sizeof c->account,sep);
        else if(!strcmp(p,"DIGIT_IRC_PASSWORD"))(void)assign(c->password,sizeof c->password,sep);
        else if(!strcmp(p,"DIGIT_IRC_MOMMY_ACCOUNT"))(void)assign(c->mommy_account,sizeof c->mommy_account,sep);
        else if(!strcmp(p,"DIGIT_IRC_AUNTIE_ACCOUNT"))(void)assign(c->auntie_account,sizeof c->auntie_account,sep);
        else if(!strcmp(p,"DIGIT_IRC_SISTER_ACCOUNT"))(void)assign(c->sister_account,sizeof c->sister_account,sep);
    }
    fclose(f);
    return config_valid(c);
}
static int write_line(SSL *ssl,const char *line) {
    size_t off=0,len=strlen(line);
    
    while(off<len) {
        int n=SSL_write(ssl,line+off,(int)(len-off));
        if(n<=0)return 0;
        off+=(size_t)n;
    }
    return 1;
}
static int command(SSL *ssl,const char *verb,const char *argument) {
    char line[1024];int n=snprintf(line,sizeof line,"%s %s\r\n",verb,argument);
    return n>0&&(size_t)n<sizeof line&&write_line(ssl,line);
}
static int connect_tls(const irc_config_t *c,SSL_CTX **context,SSL **session) {
    struct addrinfo hints={0},*addresses=NULL,*p;
    SSL_CTX *ctx=NULL;SSL *ssl=NULL;int fd=-1;
    hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
    if(getaddrinfo(c->server,c->port,&hints,&addresses)){return -1;}
    for(p=addresses;p;p=p->ai_next) {
        fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if(fd>=0&&connect(fd,p->ai_addr,p->ai_addrlen)==0)break;
        if(fd>=0)close(fd);
        fd=-1;
    }
    freeaddrinfo(addresses);if(fd<0){return -1;}
    
    /* Keep TLS reads blocking: SO_RCVTIMEO can make OpenSSL report an
     * idle read as SSL_ERROR_SYSCALL, provoking a reconnect loop. */
    ctx=SSL_CTX_new(TLS_client_method());
    if(!ctx||SSL_CTX_set_default_verify_paths(ctx)!=1)goto fail;
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);
    ssl=SSL_new(ctx);
    if(!ssl||SSL_set_fd(ssl,fd)!=1||
        SSL_set_tlsext_host_name(ssl,c->server)!=1||
        SSL_set1_host(ssl,c->server)!=1||
        SSL_connect(ssl)!=1||SSL_get_verify_result(ssl)!=X509_V_OK)goto fail;
    *context=ctx;*session=ssl;return fd;
fail:
    
    if(ssl)SSL_free(ssl);
    if(ctx)SSL_CTX_free(ctx);
    close(fd);return -1;
}
static int authenticate(SSL *ssl,const irc_config_t *c) {
    unsigned char plain[512];char encoded[720],line[750];
    size_t a=strlen(c->account),p=strlen(c->password);int n,ok;
    if(a+p+2>sizeof plain)return 0;
    plain[0]=0;memcpy(plain+1,c->account,a);plain[a+1]=0;
    memcpy(plain+a+2,c->password,p);
    n=EVP_EncodeBlock((unsigned char *)encoded,plain,(int)(a+p+2));
    OPENSSL_cleanse(plain,sizeof plain);
    if(n<0||n>400){OPENSSL_cleanse(encoded,sizeof encoded);return 0;}
    (void)snprintf(line,sizeof line,"AUTHENTICATE %s\r\n",encoded);
    OPENSSL_cleanse(encoded,sizeof encoded);
    ok=write_line(ssl,line);
    OPENSSL_cleanse(line,sizeof line);
    return ok;
}
/* IRCv3 account-tag is supplied by Libera.Chat after CAP ACK. Never
 * treat a nickname or user-controlled message as an authenticated account. */
/* Each retained DSD event is transmitted as its own IRC PRIVMSG.
 * The ABI diagnosis contains the full event array; its 512-byte summary
 * is deliberately not used as a transport for the complete trace. */
static int dsd_send_line(SSL *ssl,const char *target,const char *body){
 char wire[850];int n;
 size_t size=strlen(body);
 if(size>350||!reply_valid(body,size))return 0;
 n=snprintf(wire,sizeof wire,"PRIVMSG %s :%s\r\n",target,body);
 if(n<=0||(size_t)n>=sizeof wire)return 0;
 if(!write_line(ssl,wire))return 0;
 /* Avoid blasting up to 34 lines into IRC in a single burst. */
 {struct timespec delay={0,200000000L};(void)nanosleep(&delay,NULL);}
 return 1;
}
static int dsd_send_full_trace(SSL *ssl,const char *target,const char *account,int explain){
 digit_dsd_query_t request={0};
 digit_dsd_result_t result={0};
 char line[400];size_t used=0;unsigned i;
 if(!account||!account[0]||!target)return 0;
 request.trace_id=dsd_previous(account);
 if(!request.trace_id)
  return dsd_send_line(ssl,target,"DSD: Previous private request trace unavailable.");
 if(host_api->invoke_service(DIGIT_DSD_DIAGNOSE_SERVICE,&request,sizeof request,
    &result,sizeof result,&used)!=STNLABZ_MODULE_OK||used!=sizeof result)
  return dsd_send_line(ssl,target,"DSD: Trace retrieval failed.");
 if(!result.found)
  return dsd_send_line(ssl,target,"DSD: Previous request trace is no longer retained.");
 if(explain){
  digit_dsd_report_request_t report_request={0};
  digit_dsd_report_result_t report={0};
  size_t received=0;
  const char *origin=dsd_origin(account,request.trace_id);
  report_request.trace_id=request.trace_id;
  snprintf(report_request.origin,sizeof report_request.origin,"%s",origin);
  if(host_api->invoke_service(DIGIT_DSD_REPORT_SERVICE,
       &report_request,sizeof report_request,&report,sizeof report,
       &received)!=STNLABZ_MODULE_OK||received!=sizeof report||
       report.count>DIGIT_DSD_REPORT_LINES)
   return dsd_send_line(ssl,target,"DSD: Diagnostic report service unavailable.");
  for(i=0;i<report.count;i++){
   if(!memchr(report.lines[i],0,sizeof report.lines[i])||
      !dsd_send_line(ssl,target,report.lines[i]))return 0;
  }
  return 1;
 }
 snprintf(line,sizeof line,"DSD trace %llu: %u recorded event(s), %u failure(s), %u dropped.",
    (unsigned long long)result.trace_id,result.count,result.failures,result.dropped);
 if(!dsd_send_line(ssl,target,line))return 0;
 for(i=0;i<result.count&&i<DIGIT_DSD_MAX_EVENTS;i++){
  const digit_dsd_event_t *e=&result.events[i];
  snprintf(line,sizeof line,
    "DSD %llu [%u/%u] %s.%s -> %s; status=%s; records=%u; source=%s:%u",
    (unsigned long long)result.trace_id,i+1,result.count,
    e->module,e->method,e->operation,e->status,e->records,e->file,e->line);
  if(!dsd_send_line(ssl,target,line))return 0;
 }
 snprintf(line,sizeof line,"DSD trace %llu complete: %u event(s) displayed; %u dropped.",
    (unsigned long long)result.trace_id,i,result.dropped);
 return dsd_send_line(ssl,target,line);
}
/* The Observation module owns the write. Always preserve its disposition;
 * never claim that IRC accepted a message as persisted merely because the
 * transport received it. No message content or account is written to logs. */
static stnlabz_module_result_t submit_observation(
 const stnlabz_module_host_t *api,const digit_observation_record_t *rec){
 size_t used=0;
 digit_observation_receipt_t receipt={0};
 stnlabz_module_result_t rc;
 if(!api||!api->invoke_service||!rec)
     return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 rc=api->invoke_service(DIGIT_OBSERVATION_RECORD_SERVICE,
     rec,sizeof *rec,NULL,0,&used);
 /* The separately deployed Observation v0.1.3 service rejects the current
  * public envelope with INVALID_ARGUMENT. SQLite3 owns the same ABI struct
  * and atomically persists Observation + context. Fall back ONLY on that
  * explicit rejection, never after a successful insert or unknown failure. */
 if(rc==STNLABZ_MODULE_ERR_INVALID_ARGUMENT){
     syslog(LOG_WARNING,
       "digit.irc observation_route=sqlite3_direct reason=observation_invalid_argument");
     used=0;
     rc=api->invoke_service(DIGIT_OBSERVATION_INSERT_SERVICE,
          rec,sizeof *rec,&receipt,sizeof receipt,&used);
     if(rc==STNLABZ_MODULE_OK &&
        (used!=sizeof receipt || receipt.observation_id<=0 ||
         receipt.context_id<=0 || !memchr(receipt.event_id,0,sizeof receipt.event_id) ||
         !receipt.event_id[0]))
         rc=STNLABZ_MODULE_ERR_INVALID_STATE;
     syslog(rc==STNLABZ_MODULE_OK?LOG_INFO:LOG_ERR,
         "digit.irc observation_route=sqlite3_direct completed=1 status=%d verified=%d observation_id=%lld context_id=%lld",
         (int)rc,rc==STNLABZ_MODULE_OK,
         (long long)receipt.observation_id,(long long)receipt.context_id);
     return rc;
 }
 if(rc!=STNLABZ_MODULE_OK)return rc;
 return used==0?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static int observation_fixture_mode;
static unsigned observation_fixture_primary,observation_fixture_fallback;
static stnlabz_module_result_t observation_fixture_invoke(
 const char *service,const void *request,size_t size,
 void *response,size_t capacity,size_t *used){
 (void)response;(void)capacity;
 if(used)*used=0;
 if(!service||!request||size!=sizeof(digit_observation_record_t))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!strcmp(service,DIGIT_OBSERVATION_INSERT_SERVICE)){
    digit_observation_receipt_t receipt={0};
    ++observation_fixture_fallback;
    if(observation_fixture_mode!=4 && observation_fixture_mode!=5)
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    if(!response||capacity<sizeof receipt||!used)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if(observation_fixture_mode==4){
       receipt.observation_id=111;
       receipt.context_id=222;
       snprintf(receipt.event_id,sizeof receipt.event_id,"irc-fixture-111");
    }
    memcpy(response,&receipt,sizeof receipt);
    *used=sizeof receipt;
    return STNLABZ_MODULE_OK;
 }
 if(strcmp(service,DIGIT_OBSERVATION_RECORD_SERVICE))
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 ++observation_fixture_primary;
 if(observation_fixture_mode==1)return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(observation_fixture_mode==2)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(observation_fixture_mode==3){
    if(used)*used=1;
    return STNLABZ_MODULE_OK;
 }
 if(observation_fixture_mode==4||observation_fixture_mode==5)
    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 return STNLABZ_MODULE_OK;
}
static void observation_status(stnlabz_module_result_t rc) {
 syslog(rc==STNLABZ_MODULE_OK?LOG_INFO:LOG_ERR,
    "digit.irc observation_service status=%d accepted=%d",
    (int)rc,rc==STNLABZ_MODULE_OK);
}
/* Capture every channel PRIVMSG before intent/mention filtering.
 * Observation owns persistence; IRC only submits the transport envelope.
 * Private messages and non-channel traffic are never journaled. */
static void observe_channel(const irc_config_t *cfg,const char *raw,int tags_enabled){
 char copy[2048],*p,*end,*tags,*prefix,*cmd,*target,*message,*bang;
 digit_observation_record_t rec={0};
 if(!cfg||!raw||!tags_enabled||strlen(raw)>=sizeof copy||
    !host_api||!host_api->invoke_service)return;
 memcpy(copy,raw,strlen(raw)+1);p=copy;
 if(*p=='@'){
  tags=++p;end=strchr(tags,' ');
  if(!end)return;
  *end++=0;
  p=end;
  while(tags&&*tags){
   char *next=strchr(tags,';');
   if(next)*next++=0;
   if(!strncmp(tags,"account=",8)&&strcmp(tags+8,"*")&&
      strlen(tags+8)<sizeof rec.account)
    snprintf(rec.account,sizeof rec.account,"%s",tags+8);
   tags=next;
  }
 }
 if(*p!=':')return;
 prefix=++p;end=strchr(prefix,' ');
 if(!end)return;
 *end++=0;
 p=end;
 bang=strchr(prefix,'!');if(!bang)return;*bang=0;
 if(!*prefix||strlen(prefix)>=sizeof rec.sender)return;
 cmd=p;end=strchr(cmd,' ');if(!end)return;
 *end++=0;
 if(strcmp(cmd,"PRIVMSG"))return;
 target=end;end=strchr(target,' ');if(!end)return;
 *end++=0;
 if(strcasecmp(target,cfg->channel)||*end!=':')return;
 message=end+1;
 if(!*message||strlen(message)>=sizeof rec.text)return;
 snprintf(rec.transport,sizeof rec.transport,"irc");
 snprintf(rec.channel,sizeof rec.channel,"%s",cfg->channel);
 snprintf(rec.sender,sizeof rec.sender,"%s",prefix);
 snprintf(rec.text,sizeof rec.text,"%s",message);
 observation_status(submit_observation(host_api,&rec));
}
static const char *greeting_kind(const irc_config_t *cfg,const char *account,const char *nick);
/* A family member can address Digit in the middle of an utterance.
 * Do not convert all ambient family talk into an invitation. */
static int family_reference(const char *message){
 const char *p;
 static const char *const references[]={"digit","digicat","kitten"};
 size_t i;
 if(!message)return 0;
 for(p=message;*p;p++){
  for(i=0;i<sizeof references/sizeof references[0];i++){
   size_t n=strlen(references[i]);
   if((p==message||!isalnum((unsigned char)p[-1]))&&
      !strncasecmp(p,references[i],n)&&
      (!p[n]||!isalnum((unsigned char)p[n])))return 1;
  }
 }
 return 0;
}
static int incoming(SSL *ssl,const irc_config_t *cfg,char *line,int tags_enabled) {
    char *sender,*bang,*cmd,*target,*body,*space;
    char response[512],wire[1024];
    irc_input_t input={0};
    size_t used=0;
    int k,private_message,passive_channel=0;
    if (!tags_enabled || line[0]!='@') return 1;
    {
        char *end=strchr(line,' ');
        char *tag=line+1,*next;
        if(!end)return 1;
        *end=0;
        while(tag && *tag) {
            next=strchr(tag,';');
            if(next)*next++=0;
            if(!strncmp(tag,"account=",8) && tag[8] &&
                strcmp(tag+8,"*") && strlen(tag+8)<sizeof input.account)
                snprintf(input.account,sizeof input.account,"%s",tag+8);
            tag=next;
        }
        line=end+1;
    }
    if(line[0]!=':' || !input.account[0])return 1;
    sender=line+1;bang=strchr(sender,'!');if(!bang)return 1;
    *bang=0;space=strchr(bang+1,' ');if(!space)return 1;
    cmd=space+1;if(strncmp(cmd,"PRIVMSG ",8))return 1;
    target=cmd+8;space=strchr(target,' ');if(!space)return 1;
    *space++=0;if(*space!=':')return 1;body=space+1;
    private_message=!strcasecmp(target,cfg->nick);
    if(private_message) target=sender;
    else {
        if(strcasecmp(target,cfg->channel))return 1;
        {
            size_t prefix=address_prefix(body,cfg->nick);
            if(prefix){
                body+=prefix;
                if(*body==':'||*body==',')body++;
                while(*body==' ')body++;
                /* Bare address remains an empty contact message. */
            }else{
                passive_channel=1;
            }
        }
    }
    snprintf(input.sender,sizeof input.sender,"%s",sender);
    snprintf(input.target,sizeof input.target,"%s",target);
    snprintf(input.message,sizeof input.message,"%s",body);
    if(!host_api||!host_api->invoke_service)return 1;
    if(private_message&&
       (!strcmp(input.message,"show trace")||
        !strcmp(input.message,"diagnose my last request"))){
        /* Only the authenticated account's previous private trace is exposed.
         * Transport the entire retained event array as numbered IRC lines. */
        return dsd_send_full_trace(ssl,target,input.account,!strcmp(input.message,"diagnose my last request"));
    }
    if(private_message&&dsd_command(input.message)){
        /* Command is never invoked from a channel, even when mentioned. */
        if(!dsd_dm_reply(input.account,input.message,response,sizeof response,&used))
            return 1;
        if(dsd_tracks_previous(input.message)){
            uint64_t trace=dsd_next_trace++;
            if(!trace)trace=dsd_next_trace++;
            dsd_record_command(input.account,trace,
                !strcmp(response,"DSD is unavailable or not active.")?
                  "SERVICE_UNAVAILABLE":"RESPONSE_RETURNED",__LINE__);
        }
    }else{
        stnlabz_module_result_t rc;
        uint64_t trace=0;
        unsigned observation_line=__LINE__+1;
        if(private_message){
            size_t trace_used=0,seen_size=0;
            uint64_t seen=0,query_token=1;
            stnlabz_module_result_t bound;
            trace=dsd_next_trace++;
            if(!trace)trace=dsd_next_trace++;
            bound=host_api->invoke_service(DIGIT_DSD_BIND_SERVICE,
                &trace,sizeof trace,NULL,0,&trace_used);
            if(bound==STNLABZ_MODULE_OK){
                bound=host_api->invoke_service(DIGIT_DSD_CURRENT_SERVICE,
                    &query_token,sizeof query_token,&seen,sizeof seen,&seen_size);
            }
            if(bound!=STNLABZ_MODULE_OK||seen_size!=sizeof seen||seen!=trace){
                digit_dsd_record_t warning={0};
                size_t warning_used=0;
                warning.trace_id=warning.event.trace_id=trace;
                warning.event.span_id=1;
                warning.event.result=1;
                warning.event.line=__LINE__;
                snprintf(warning.event.module,sizeof warning.event.module,"irc");
                snprintf(warning.event.method,sizeof warning.event.method,"incoming");
                snprintf(warning.event.file,sizeof warning.event.file,"modules/irc/irc.c");
                snprintf(warning.event.operation,sizeof warning.event.operation,"request_trace");
                snprintf(warning.event.status,sizeof warning.event.status,"TRACE_BIND_FAILED");
                (void)host_api->invoke_service(DIGIT_DSD_RECORD_SERVICE,
                    &warning,sizeof warning,NULL,0,&warning_used);
            }
        }
        /* Account-authenticated family messages may receive a Socials
         * response. All other messages continue through Conversation. */
        rc=STNLABZ_MODULE_ERR_NOT_FOUND;
        if(!passive_channel || family_reference(input.message)){
            const char *family=greeting_kind(cfg,input.account,input.sender);
            if(family && strcmp(family,"unknown")){
                digit_socials_family_request_t social={{0}};
                digit_socials_family_response_t social_response={{0}};
                size_t social_used=0;
                snprintf(social.kind,sizeof social.kind,"%s",family);
                snprintf(social.text,sizeof social.text,"%s",input.message);
                rc=host_api->invoke_service(DIGIT_SOCIALS_FAMILY_SERVICE,
                    &social,sizeof social,&social_response,
                    sizeof social_response,&social_used);
                if(rc==STNLABZ_MODULE_OK&&
                   social_used==sizeof social_response&&
                   memchr(social_response.text,0,sizeof social_response.text)){
                    used=strlen(social_response.text);
                    if(used<sizeof response)
                        memcpy(response,social_response.text,used+1);
                    else rc=STNLABZ_MODULE_ERR_INVALID_STATE;
                }else if(rc==STNLABZ_MODULE_OK){
                    rc=STNLABZ_MODULE_ERR_INVALID_STATE;
                }
            }
        }
        /* The direct IRC message service may route a non-family smile
         * into factual clarification. For explicitly addressed contact,
         * Socials owns the expressive response for every account. */
        if(rc!=STNLABZ_MODULE_OK&&!passive_channel&&
           social_emoticon(input.message)){
            digit_socials_request_t expression_request={{0}};
            digit_socials_response_t expression={{0}};
            size_t expression_used=0;
            snprintf(expression_request.category,
                     sizeof expression_request.category,"cats");
            rc=host_api->invoke_service(DIGIT_SOCIALS_SERVICE,
                &expression_request,sizeof expression_request,
                &expression,sizeof expression,&expression_used);
            if(rc==STNLABZ_MODULE_OK&&
               expression_used==sizeof expression&&
               memchr(expression.expression,0,sizeof expression.expression)){
                used=strlen(expression.expression);
                if(used<sizeof response)
                    memcpy(response,expression.expression,used+1);
                else rc=STNLABZ_MODULE_ERR_INVALID_STATE;
            }else rc=STNLABZ_MODULE_ERR_NOT_FOUND;
        }
        if(rc!=STNLABZ_MODULE_OK){
            used=0;
            rc=host_api->invoke_service(
                passive_channel?IRC_CONVERSATION_LISTEN:IRC_INPUT_SERVICE,
                &input,sizeof input,response,sizeof response,&used);
        }
        if(private_message){
            uint64_t clear=0;size_t trace_used=0;
            (void)host_api->invoke_service(DIGIT_DSD_BIND_SERVICE,
                &clear,sizeof clear,NULL,0,&trace_used);
            dsd_record_message(input.account,trace,
                rc==STNLABZ_MODULE_OK&&used>0,rc,
                observation_line);
            dsd_remember_origin(input.account,trace,input.message);
        }
        if(rc!=STNLABZ_MODULE_OK)return 1;
    }
    /* Core returns inventory records separated by ASCII RS (0x1e).
     * IRC cannot transmit multiline payloads: send one PRIVMSG per record.
     * Reject empty records and CR/LF injection; never interpret remote text
     * as wire commands. */
    if(!used || used>=sizeof response)return 1;
    response[used]=0;
    {
        char *part=response;
        char *end=response+used;
        while(part<end) {
            char *separator=memchr(part,0x1e,(size_t)(end-part));
            size_t length=(size_t)((separator?separator:end)-part);
            if(!reply_valid(part,length))return 1;
            k=snprintf(wire,sizeof wire,"PRIVMSG %s :%.*s\r\n",
                       target,(int)length,part);
            if(k<=0||(size_t)k>=sizeof wire||!write_line(ssl,wire))
                return 0;
            part=separator?separator+1:end;
        }
    }

    return 1;
}
/* JOIN notifications are transport events, not conversational intents.
 * Family classification is based only on explicitly configured IRC account
 * identities, never nickname, hostmask or channel privileges.
 */
/* A family role requires BOTH the configured authenticated account and
 * the designated IRC nickname. In particular, an account-tag mismatch
 * must never make PoeMei receive Auntie's role, or vice versa. */
static const char *greeting_kind(const irc_config_t *cfg,const char *account,
                                const char *nick){
 if(!cfg||!account||!*account||!strcmp(account,"*")||!nick||!*nick)
  return NULL;
 if(!strcasecmp(nick,"PoeMei"))
  return cfg->mommy_account[0]&&!strcasecmp(cfg->mommy_account,account)
      ?"mommy":NULL;
 if(!strcasecmp(nick,"Laurie_Vo"))
  return cfg->auntie_account[0]&&!strcasecmp(cfg->auntie_account,account)
      ?"auntie":NULL;
 /* Sister's nickname is not designated in this transport; the registered
  * account remains the sole source for that configured relationship. */
 if(cfg->sister_account[0]&&!strcasecmp(cfg->sister_account,account))
  return "sister";
 return "unknown";
}
static int join_greeting(SSL *ssl,const irc_config_t *cfg,char *line,
 int tags_enabled,int channel_ready){
 char account[128]={0},nick[128]={0},wire[512];
 char *end,*tag,*next,*prefix,*bang,*command,*channel,*space;
 digit_socials_greeting_request_t request={0};
 digit_socials_greeting_response_t reply={0};
 size_t used=0;
 time_t now;
 const char *kind;
 int n;
 if(!tags_enabled||!channel_ready||!line||line[0]!='@')return 1;
 end=strchr(line,' ');if(!end)return 1;
 *end=0;tag=line+1;
 while(tag&&*tag){
  next=strchr(tag,';');if(next)*next++=0;
  if(!strncmp(tag,"account=",8)&&tag[8]&&
     strlen(tag+8)<sizeof account)
   snprintf(account,sizeof account,"%s",tag+8);
  tag=next;
 }
 prefix=end+1;
 if(*prefix!=':')return 1;
 prefix++;bang=strchr(prefix,'!');if(!bang)return 1;
 *bang=0;
 if(strlen(prefix)>=sizeof nick)return 1;
 snprintf(nick,sizeof nick,"%s",prefix);
 space=strchr(bang+1,' ');if(!space)return 1;
 command=space+1;
 if(strncmp(command,"JOIN ",5))return 1;
 channel=command+5;
 space=strchr(channel,' ');if(space)*space=0;
 if(strcasecmp(channel,cfg->channel)||!strcasecmp(nick,cfg->nick))return 1;
 kind=greeting_kind(cfg,account,nick);
 if(!kind||!host_api||!host_api->invoke_service)return 1;
 now=time(NULL);
 if(now==(time_t)-1||(last_greeting&&now>=last_greeting&&
   now-last_greeting<IRC_GREETING_INTERVAL))return 1;
 snprintf(request.kind,sizeof request.kind,"%s",kind);
 if(host_api->invoke_service(DIGIT_SOCIALS_GREETING_SERVICE,&request,
    sizeof request,&reply,sizeof reply,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof reply)return 1;
 n=(int)strnlen(reply.text,sizeof reply.text);
 if(n<=0||n>350||!reply_valid(reply.text,(size_t)n))return 1;
 n=snprintf(wire,sizeof wire,"PRIVMSG %s :%s\r\n",cfg->channel,reply.text);
 if(n<=0||(size_t)n>=sizeof wire)return 1;
 if(!write_line(ssl,wire))return 0;
 last_greeting=now;
 return 1;
}
static void session(const irc_config_t *cfg) {
    SSL_CTX *ctx=NULL;SSL *ssl=NULL;int fd,authed=0,joined=0,got_sasl=0,welcomed=0,tags_enabled=0;
    char line[2048];size_t used=0;
    last_greeting=0;
    fd=connect_tls(cfg,&ctx,&ssl);if(fd<0)return;
    pthread_mutex_lock(&socket_mutex);
    active_socket=fd;
    pthread_mutex_unlock(&socket_mutex);
    
    if(!write_line(ssl,"CAP LS 302\r\n")||
        !command(ssl,"NICK",cfg->nick)||
        !command(ssl,"USER","digit 0 * :Digit IRC module"))goto done;
    while(!stopping) {
        fd_set readfds;int rc;struct timeval timeout={1,0};
        char ch;int n;
        irc_s2_report_t pending;int have=0;
        pthread_mutex_lock(&outbound_mutex);
        if(outbound_count){pending=outbound[outbound_head];have=1;}
        pthread_mutex_unlock(&outbound_mutex);
        if(have){
            char wire[512];int k=snprintf(wire,sizeof wire,"PRIVMSG %s :%s\r\n",cfg->channel,pending.report);
            if(k<=0||(size_t)k>=sizeof wire||!write_line(ssl,wire))break;
            pthread_mutex_lock(&outbound_mutex);
            outbound_head=(outbound_head+1)%S2_QUEUE_CAP;outbound_count--;
            pthread_mutex_unlock(&outbound_mutex);
            continue;
        }
        FD_ZERO(&readfds);FD_SET(fd,&readfds);
        if(wake_pipe[0]>=0)FD_SET(wake_pipe[0],&readfds);
        if(!SSL_pending(ssl)){
            rc=select((fd>wake_pipe[0]?fd:wake_pipe[0])+1,&readfds,NULL,NULL,&timeout);
            if(rc<0){if(errno==EINTR)continue;break;}
            if(rc==0)continue;
            if(wake_pipe[0]>=0&&FD_ISSET(wake_pipe[0],&readfds)){
                unsigned char bytes[128];
                 ssize_t drained=read(wake_pipe[0],bytes,sizeof bytes);
                 if(drained<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR)
                     syslog(LOG_WARNING,"digit.irc wake drain failed: %d",errno);
                continue;
            }
            if(!FD_ISSET(fd,&readfds))continue;
        }
        n=SSL_read(ssl,&ch,1);
        if(n<=0){int error=SSL_get_error(ssl,n);if(error==SSL_ERROR_WANT_READ)continue;break;}
        if(ch=='\r')continue;
        if(ch!='\n') {
            if(used+1>=sizeof line){used=0;continue;}
            line[used++]=ch;continue;
        }
        line[used]=0;used=0;
        
        if(!strncmp(line,"PING :",6)) {
            if(!command(ssl,"PONG",line+5))break;
        } else if(strstr(line," CAP ")&&strstr(line," LS ")&&strstr(line,"sasl")) {
            
            if(!strstr(line,"account-tag") || !write_line(ssl,"CAP REQ :sasl account-tag\r\n")){break;}
            
        } else if(strstr(line," CAP ")&&strstr(line," ACK ")&&strstr(line,"sasl")) {
            got_sasl=1;tags_enabled=strstr(line,"account-tag")!=NULL;
            
            if(!tags_enabled || !write_line(ssl,"AUTHENTICATE PLAIN\r\n")){break;}
            
        } else if(got_sasl&&!strcmp(line,"AUTHENTICATE +")) {
            
            if(!authenticate(ssl,cfg)){break;}
            
        } else if(strstr(line," 903 ")) {
            
            authed=1;if(!write_line(ssl,"CAP END\r\n")){break;}
            if(welcomed&&!joined){if(!command(ssl,"JOIN",cfg->channel)){break;}joined=1;}
        } else if(strstr(line," 904 ")||strstr(line," 905 ")||strstr(line," 906 ")||strstr(line," 907 ")) {
            break;
        } else if(strstr(line," 001 ")) {
            welcomed=1;
            if(authed&&!joined){if(!command(ssl,"JOIN",cfg->channel)){break;}joined=1;}
        } else if(strstr(line," 433 ")) {
            
        } else if(strstr(line," 471 ")||strstr(line," 473 ")||
                  strstr(line," 474 ")||strstr(line," 475 ")) {
            
        } else if(authed&&joined&&strstr(line," JOIN ")&&strstr(line,cfg->nick)) {
            pthread_mutex_lock(&outbound_mutex);irc_ready=1;pthread_mutex_unlock(&outbound_mutex);
            if(!join_greeting(ssl,cfg,line,tags_enabled,joined))break;
        } else if(authed&&joined&&strstr(line," JOIN ")) {
            if(!join_greeting(ssl,cfg,line,tags_enabled,joined))break;
        } else if(authed&&strstr(line," PRIVMSG ")) {
            observe_channel(cfg,line,tags_enabled);
            if(!incoming(ssl,cfg,line,tags_enabled))break;
        }
    }
done:
    pthread_mutex_lock(&outbound_mutex);irc_ready=0;pthread_mutex_unlock(&outbound_mutex);
    pthread_mutex_lock(&socket_mutex);
    active_socket=-1;
    pthread_mutex_unlock(&socket_mutex);
    if(ssl){if(!stopping)SSL_shutdown(ssl);SSL_free(ssl);}
    if(ctx)SSL_CTX_free(ctx);
    close(fd);
}
static void *worker_main(void *unused) {
    irc_config_t cfg; (void)unused;
    if(!config_read(&cfg)){return NULL;}
    
    while(!stopping) {
        unsigned i;session(&cfg);
        for(i=0;i<10&&!stopping;i++)sleep(1);
    }
    OPENSSL_cleanse(&cfg,sizeof cfg);
    return NULL;
}
/* [AI:GPT-6 | 2026-10-18] Offline, deterministic transport qualification.
 * Each condition is executed; negative cases verify rejection paths.
 */
static void check(stnlabz_module_qualification_result_t *r,int ok,int negative) {
    r->tests_executed++;
    if(ok)r->tests_passed++;else r->tests_failed++;
    if(negative){r->negative_test_executed=1;if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *result) {
    irc_config_t c={0};
    char target[8];
    const char valid[]="Hello";
    const char cr[]="bad\r";
    const char lf[]="bad\n";
    const char nul[]={'a','\0','b'};
    if(!result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(result,0,sizeof *result);
    check(result,assign(target,sizeof target,"test")&&!strcmp(target,"test"),0);
    check(result,assign(target,sizeof target,"'foo'")&&!strcmp(target,"foo"),0);
    check(result,assign(target,sizeof target,"\"bar\"")&&!strcmp(target,"bar"),0);
    check(result,!assign(target,sizeof target,""),1);
    check(result,!assign(target,sizeof target,"12345678"),1);
    check(result,!assign(target,sizeof target,"''"),1);
    (void)assign(c.server,sizeof c.server,"irc.libera.chat");
    (void)assign(c.port,sizeof c.port,"6697");
    (void)assign(c.channel,sizeof c.channel,"#stn-labz");
    (void)assign(c.nick,sizeof c.nick,"Digit");
    (void)assign(c.account,sizeof c.account,"Digit");
    (void)assign(c.password,sizeof c.password,"fixture-only");
    check(result,config_valid(&c),0);
    check(result,!config_valid(NULL),1);
    (void)assign(c.server,sizeof c.server,"evil.example");
    check(result,!config_valid(&c),1);
    (void)assign(c.server,sizeof c.server,"irc.libera.chat");
    (void)assign(c.port,sizeof c.port,"6667");
    check(result,!config_valid(&c),1);
    (void)assign(c.port,sizeof c.port,"6697");
    (void)assign(c.channel,sizeof c.channel,"#other");
    check(result,!config_valid(&c),1);
    (void)assign(c.channel,sizeof c.channel,"#stn-labz");
    (void)assign(c.nick,sizeof c.nick,"bad nick");
    check(result,!config_valid(&c),1);
    (void)assign(c.nick,sizeof c.nick,"Digit");
    check(result,dsd_command("show trace"),0);
    check(result,!dsd_tracks_previous("show trace"),1);
    check(result,dsd_tracks_previous("show dsd"),0);
    check(result,!dsd_tracks_previous("diagnose my last request"),1);
    check(result,!dsd_tracks_previous("diagnose trace 10"),1);
    check(result,!dsd_tracks_previous("show modules"),1);
    check(result,family_reference("there you are kitten"),0);
    check(result,family_reference("awww Digit :)"),0);
    check(result,!family_reference("more coffee"),1);
    check(result,!family_reference("digitized"),1);
    check(result,social_emoticon(":)"),0);
    check(result,social_emoticon(":xD"),0);
    check(result,social_emoticon("=^.^="),0);
    check(result,!social_emoticon("what is C?"),1);
    check(result,!social_emoticon("hello"),1);
    check(result,!social_emoticon(""),1);
    check(result,address_prefix("digit :)","digicat")==5,0);
    check(result,address_prefix("Digit :)","digicat")==5,0);
    check(result,address_prefix("digicat :)","digicat")==7,0);
    check(result,address_prefix("digit: :)","digicat")==5,0);
    check(result,address_prefix("digit, :)","digicat")==5,0);
    check(result,address_prefix("digit?","digicat")==5,0);
    check(result,address_prefix("digit who is your creator?","digicat")==5,0);
    check(result,address_prefix("digicat who are you?","digicat")==7,0);
    check(result,address_prefix("digital keyboard","digicat")==0,1);
    check(result,address_prefix("DigitXYZ: hello","Digit")==0,1);
    check(result,address_prefix("Di","Digit")==0,1);
    check(result,address_prefix(NULL,"Digit")==0,1);
    check(result,address_prefix("unrelated","")==0,1);
    check(result,reply_valid(valid,sizeof valid-1),0);
    check(result,!reply_valid(cr,sizeof cr-1),1);
    check(result,!reply_valid(lf,sizeof lf-1),1);
    check(result,!reply_valid(nul,sizeof nul),1);
    {irc_s2_report_t test_report={{0}};
     snprintf(test_report.report,sizeof test_report.report,"[S2] test report");
     check(result,safe_report(test_report.report,sizeof test_report.report),0);
     snprintf(test_report.report,sizeof test_report.report,"bad\nline");
     check(result,!safe_report(test_report.report,sizeof test_report.report),1);
     memset(test_report.report,'X',sizeof test_report.report);
     check(result,!safe_report(test_report.report,sizeof test_report.report),1);
    }
    /* Dispatch outcomes are tested against the actual ABI shape.
     * A service success with an unexpected response size is rejected. */
    {
      digit_observation_record_t rec={0};
      stnlabz_module_host_t mock={0};
      mock.invoke_service=observation_fixture_invoke;
      check(result,submit_observation(NULL,&rec)==
        STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      check(result,submit_observation(&mock,NULL)==
        STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
      observation_fixture_mode=0;
      check(result,submit_observation(&mock,&rec)==STNLABZ_MODULE_OK,0);
      observation_fixture_mode=1;
      check(result,submit_observation(&mock,&rec)==
        STNLABZ_MODULE_ERR_NOT_FOUND,1);
      observation_fixture_mode=2;
      check(result,submit_observation(&mock,&rec)==
        STNLABZ_MODULE_ERR_INVALID_STATE,1);
      observation_fixture_mode=3;
      check(result,submit_observation(&mock,&rec)==
        STNLABZ_MODULE_ERR_INVALID_STATE,1);
      observation_fixture_mode=4;
      observation_fixture_primary=0;observation_fixture_fallback=0;
      check(result,submit_observation(&mock,&rec)==STNLABZ_MODULE_OK &&
        observation_fixture_primary==1 && observation_fixture_fallback==1,0);
      observation_fixture_mode=5;
      check(result,submit_observation(&mock,&rec)==
        STNLABZ_MODULE_ERR_INVALID_STATE,1);
      observation_fixture_mode=0;
      observation_fixture_primary=0;observation_fixture_fallback=0;
      check(result,submit_observation(&mock,&rec)==STNLABZ_MODULE_OK &&
        observation_fixture_primary==1 && observation_fixture_fallback==0,0);
      observation_fixture_mode=0;
    }
    strcpy(c.mommy_account,"PoeAccount");
    strcpy(c.auntie_account,"LaurieAccount");
    check(result,!strcmp(greeting_kind(&c,"PoeAccount","PoeMei"),"mommy"),0);
    check(result,!strcmp(greeting_kind(&c,"LaurieAccount","Laurie_Vo"),"auntie"),0);
    check(result,greeting_kind(&c,"LaurieAccount","PoeMei")==NULL,1);
    check(result,greeting_kind(&c,"PoeAccount","Laurie_Vo")==NULL,1);
    check(result,greeting_kind(&c,"PoeAccount","OtherNick")==NULL ||
        strcmp(greeting_kind(&c,"PoeAccount","OtherNick"),"mommy"),1);
    check(result,greeting_kind(&c,"","PoeMei")==NULL,1);
    check(result,greeting_kind(&c,"*","Laurie_Vo")==NULL,1);
    check(result,greeting_kind(&c,"LaurieAccount","PoeMei")==NULL,1);
    check(result,greeting_kind(&c,"","PoeMei") == NULL,1);
    check(result,!strcmp(greeting_kind(&c,"registered","Unknown"),"unknown"),0);
    strcpy(c.auntie_account,"LaurieAccount");
    check(result,!strcmp(greeting_kind(&c,"LaurieAccount","Laurie_Vo"),"auntie"),0);
    check(result,!strcmp(greeting_kind(&c,"Laurie_Vo","Laurie_Vo"),"unknown"),1);
    /* RFC 4616 PLAIN fixture: empty authorization identity, account,
     * password. Verify the same construction and base64 primitive as the
     * production SASL path without using or logging actual credentials. */
    {
        const unsigned char expected[]={'\0','u','s','e','r','\0','p','a','s','s'};
        unsigned char plain[sizeof expected]={0};
        char encoded[32]={0};
        unsigned char decoded[32]={0};
        int length,decoded_length;
        memcpy(plain+1,"user",4);
        memcpy(plain+6,"pass",4);
        length=EVP_EncodeBlock((unsigned char *)encoded,plain,(int)sizeof plain);
        decoded_length=length>0?EVP_DecodeBlock(decoded,
            (const unsigned char *)encoded,length):-1;
        check(result,length==16&&!strcmp(encoded,"AHVzZXIAcGFzcw=="),0);
        check(result,decoded_length==12&&
            !memcmp(decoded,expected,sizeof expected),0);
        OPENSSL_cleanse(plain,sizeof plain);
        OPENSSL_cleanse(decoded,sizeof decoded);
        OPENSSL_cleanse(encoded,sizeof encoded);
    }

    return result->tests_executed>=20 && !result->tests_failed ?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host) {
    if(worker_started||!host||!host->invoke_service||
       !host->register_service||!host->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    host_api=host;stopping=0;
    outbound_head=0;outbound_count=0;irc_ready=0;
    memset(dsd_dm,0,sizeof dsd_dm);dsd_next_trace=1;dsd_dm_evict=0;
    if(pipe(wake_pipe)!=0){host_api=NULL;return STNLABZ_MODULE_ERR_START_FAILED;}
    (void)fcntl(wake_pipe[0],F_SETFL,O_NONBLOCK);
    (void)fcntl(wake_pipe[1],F_SETFL,O_NONBLOCK);
    if(!host->register_service(S2_DELIVER_SERVICE,deliver_service,NULL)){
        close(wake_pipe[0]);close(wake_pipe[1]);wake_pipe[0]=wake_pipe[1]=-1;
        host_api=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
    }
    if(pthread_create(&worker,NULL,worker_main,NULL)) {
        (void)host->unregister_service(S2_DELIVER_SERVICE,NULL);
        close(wake_pipe[0]);close(wake_pipe[1]);wake_pipe[0]=wake_pipe[1]=-1;
        host_api=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
    }
    worker_started=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) {
    if(!worker_started)return STNLABZ_MODULE_ERR_INVALID_STATE;
    stopping=1;
    (void)host_api->unregister_service(S2_DELIVER_SERVICE,NULL);
    pthread_mutex_lock(&socket_mutex);
    if(active_socket>=0)shutdown(active_socket,SHUT_RDWR);
    pthread_mutex_unlock(&socket_mutex);
    if(pthread_join(worker,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
    close(wake_pipe[0]);close(wake_pipe[1]);wake_pipe[0]=wake_pipe[1]=-1;
    memset(dsd_dm,0,sizeof dsd_dm);
    worker_started=0;host_api=NULL;
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
    .id="irc",.name="Libera.Chat IRC",
    .version_major=0,.version_minor=3,.version_patch=13,
    .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
    .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
    return &descriptor;
}
/* [End AI:GPT-6] */
