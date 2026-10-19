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
#include "../dsd/dsd.h"
#include "../sqlite3/sqlite3_diagnostic.h"
#include <time.h>

#define IRC_ENV_PATH "/opt/digit/state/auth/irc.env"
#define IRC_INPUT_SERVICE "digit.message"
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
    if(wake_pipe[1]>=0)(void)write(wake_pipe[1],&signal_byte,1);
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_host_t *host_api;
/* DM-only DSD bridge; trace ownership never keyed by unverified nick. */
#define DSD_DM_SLOTS 32
typedef struct {char account[128];uint64_t trace;} dsd_dm_t;
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
 record.event.result=(service_result==STNLABZ_MODULE_OK&&replied)?0:1;
 /* Record the actual ABI disposition. A non-OK return is not necessarily
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
static int nick_mention(const char *body,const char *nick) {
    size_t n;
    if(!body||!nick||!*nick)return 0;
    n=strlen(nick);
    return strlen(body)>=n && !strncasecmp(body,nick,n) &&
        (body[n]==':' || body[n]==',' || body[n]==' ');
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
static int incoming(SSL *ssl,const irc_config_t *cfg,char *line,int tags_enabled) {
    char *sender,*bang,*cmd,*target,*body,*space;
    char response[512],wire[1024];
    irc_input_t input={0};
    size_t used=0,n;
    int k,private_message;
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
    n=strlen(cfg->nick);
    private_message=!strcasecmp(target,cfg->nick);
    if(private_message) target=sender;
    else {
        if(strcasecmp(target,cfg->channel))return 1;
        if(!nick_mention(body,cfg->nick))return 1;
        body+=n;
        if(*body==':'||*body==',')body++;
        while(*body==' ')body++;
    }
    snprintf(input.sender,sizeof input.sender,"%s",sender);
    snprintf(input.target,sizeof input.target,"%s",target);
    snprintf(input.message,sizeof input.message,"%s",body);
    if(!host_api||!host_api->invoke_service)return 1;
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
        rc=host_api->invoke_service(IRC_INPUT_SERVICE,&input,sizeof input,
            response,sizeof response,&used);
        if(private_message){
            trace=dsd_next_trace++;
            if(!trace)trace=dsd_next_trace++;
            dsd_record_message(input.account,trace,
                rc==STNLABZ_MODULE_OK&&used>0,rc,
                observation_line);
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
    /* Separate on-demand SQLite3 health probe. It is not a child span
     * of the earlier message: that message may never have reached SQLite3.
     * One bounded extra private line, no channel output or record values. */
    if(private_message&&!strcmp(input.message,"diagnose my last request")){
        sqlite3_diagnostic_t db={0};
        size_t db_used=0;
        stnlabz_module_result_t db_rc=host_api->invoke_service(
          DIGIT_SQLITE3_DIAGNOSTIC_SERVICE,NULL,0,
          &db,sizeof db,&db_used);
        const char *db_text;
        if(db_rc==STNLABZ_MODULE_ERR_NOT_FOUND)
            db_text="SQLite3 probe unavailable; no database evidence was collected.";
        else if(db_rc!=STNLABZ_MODULE_OK || db_used!=sizeof db ||
                !memchr(db.state,0,sizeof db.state) ||
                !memchr(db.stage,0,sizeof db.stage) ||
                !memchr(db.detail,0,sizeof db.detail))
            db_text="SQLite3 probe failed; database status could not be verified.";
        else{
            char detail[360];
            int n=snprintf(detail,sizeof detail,
                "DSD separate SQLite3 probe: %.24s, %.44s: %.90s. This does not prove the previous request reached SQLite3.",
                db.state,db.stage,db.detail);
            if(n>0&&(size_t)n<sizeof detail && safe_report(detail,sizeof detail)){
                int w=snprintf(wire,sizeof wire,"PRIVMSG %s :%s\r\n",target,detail);
                if(w>0&&(size_t)w<sizeof wire && !write_line(ssl,wire))return 0;
            }
            return 1;
        }
        {
            int w=snprintf(wire,sizeof wire,"PRIVMSG %s :DSD: %s\r\n",target,db_text);
            if(w>0&&(size_t)w<sizeof wire && !write_line(ssl,wire))return 0;
        }
    }
    return 1;
}
/* JOIN notifications are transport events, not conversational intents.
 * Family classification is based only on explicitly configured IRC account
 * identities, never nickname, hostmask or channel privileges.
 */
static const char *greeting_kind(const irc_config_t *cfg,const char *account){
 if(!cfg||!account||!*account||!strcmp(account,"*"))return NULL;
 if(cfg->mommy_account[0]&&!strcmp(cfg->mommy_account,account))return "mommy";
 if(cfg->auntie_account[0]&&!strcmp(cfg->auntie_account,account))return "auntie";
 if(cfg->sister_account[0]&&!strcmp(cfg->sister_account,account))return "sister";
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
 kind=greeting_kind(cfg,account);
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
                unsigned char bytes[128];(void)read(wake_pipe[0],bytes,sizeof bytes);
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
    check(result,nick_mention("Digit: hello","Digit"),0);
    check(result,nick_mention("digit, hello","Digit"),0);
    check(result,!nick_mention("DigitXYZ: hello","Digit"),1);
    check(result,!nick_mention("Di","Digit"),1);
    check(result,!nick_mention(NULL,"Digit"),1);
    check(result,!nick_mention("Digit",""),1);
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
    check(result,greeting_kind(&c,"") == NULL,1);
    check(result,!strcmp(greeting_kind(&c,"registered"),"unknown"),0);
    strcpy(c.auntie_account,"LaurieAccount");
    check(result,!strcmp(greeting_kind(&c,"LaurieAccount"),"auntie"),0);
    check(result,!strcmp(greeting_kind(&c,"Laurie_Vo"),"unknown"),1);
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
    .version_major=0,.version_minor=2,.version_patch=16,
    .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
    .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
    return &descriptor;
}
/* [End AI:GPT-6] */
