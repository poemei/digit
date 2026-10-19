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
#include <sys/time.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "module.h"

#define IRC_ENV_PATH "/opt/digit/state/auth/irc.env"
#define IRC_INPUT_SERVICE "digit.message"
typedef struct {
    char server[256], port[16], nick[64], channel[128];
    char account[128], password[256];
} irc_config_t;
typedef struct {
    char sender[128], account[128], target[128], message[512];
} irc_input_t;
static pthread_t worker;
static int worker_started;
static volatile sig_atomic_t stopping;
static const stnlabz_module_host_t *host_api;

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
    if(getaddrinfo(c->server,c->port,&hints,&addresses))return -1;
    for(p=addresses;p;p=p->ai_next) {
        fd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if(fd>=0&&connect(fd,p->ai_addr,p->ai_addrlen)==0)break;
        if(fd>=0)close(fd);
        fd=-1;
    }
    freeaddrinfo(addresses);if(fd<0)return -1;
    { struct timeval timeout={1,0}; (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout); }
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
    if(a*2+p+2>sizeof plain)return 0;
    plain[0]=0;memcpy(plain+1,c->account,a);plain[a+1]=0;
    memcpy(plain+a+2,c->password,p);
    n=EVP_EncodeBlock((unsigned char *)encoded,plain,(int)(a*2+p+2));
    OPENSSL_cleanse(plain,sizeof plain);
    if(n<0||n>400){OPENSSL_cleanse(encoded,sizeof encoded);return 0;}
    (void)snprintf(line,sizeof line,"AUTHENTICATE %s\r\n",encoded);
    OPENSSL_cleanse(encoded,sizeof encoded);
    ok=write_line(ssl,line);
    OPENSSL_cleanse(line,sizeof line);
    return ok;
}
static int incoming(SSL *ssl,const irc_config_t *cfg,char *line) {
    char *sender,*bang,*cmd,*target,*body,*space;
    char response[512],wire[1024];
    irc_input_t input={0};
    size_t used=0,n;int k;
    if(line[0]!=':')return 1;
    sender=line+1;bang=strchr(sender,'!');if(!bang)return 1;
    *bang=0;space=strchr(bang+1,' ');if(!space)return 1;
    cmd=space+1;if(strncmp(cmd,"PRIVMSG ",8))return 1;
    target=cmd+8;space=strchr(target,' ');if(!space)return 1;
    *space++=0;if(*space!=':')return 1;body=space+1;
    n=strlen(cfg->nick);
    if(!strcasecmp(target,cfg->nick)) {
        target=sender;
    } else {
        if(strcasecmp(target,cfg->channel))return 1;
        if(!nick_mention(body,cfg->nick))return 1;
        body+=n;
        if(*body==':'||*body==',')body++;
        while(*body==' ')body++;
    }
    (void)snprintf(input.sender,sizeof input.sender,"%s",sender);
    (void)snprintf(input.target,sizeof input.target,"%s",target);
    (void)snprintf(input.message,sizeof input.message,"%s",body);
    /* Account is intentionally not inferred from nick; identity requires trusted IRC account tags. */
    if(!host_api||!host_api->invoke_service)return 1;
    if(host_api->invoke_service(IRC_INPUT_SERVICE,&input,sizeof input,
        response,sizeof response,&used)!=STNLABZ_MODULE_OK)return 1;
    if(!used||used>=sizeof response)return 1;
    response[used]=0;
    if(!reply_valid(response,used))return 1;
    k=snprintf(wire,sizeof wire,"PRIVMSG %s :%s\r\n",target,response);
    return k>0&&(size_t)k<sizeof wire&&write_line(ssl,wire);
}
static void session(const irc_config_t *cfg) {
    SSL_CTX *ctx=NULL;SSL *ssl=NULL;int fd,authed=0,joined=0,got_sasl=0;
    char line[2048];size_t used=0;
    fd=connect_tls(cfg,&ctx,&ssl);if(fd<0)return;
    if(!write_line(ssl,"CAP LS 302\r\n")||
        !command(ssl,"NICK",cfg->nick)||
        !command(ssl,"USER","digit 0 * :Digit IRC module"))goto done;
    while(!stopping) {
        char ch;int n=SSL_read(ssl,&ch,1);
        if(n<=0){if(SSL_get_error(ssl,n)==SSL_ERROR_WANT_READ)continue;break;}
        if(ch=='\r')continue;
        if(ch!='\n') {
            if(used+1>=sizeof line){used=0;continue;}
            line[used++]=ch;continue;
        }
        line[used]=0;used=0;
        if(!strncmp(line,"PING :",6)) {
            if(!command(ssl,"PONG",line+5))break;
        } else if(strstr(line," CAP ")&&strstr(line," LS ")&&strstr(line,"sasl")) {
            if(!write_line(ssl,"CAP REQ :sasl\r\n"))break;
        } else if(strstr(line," CAP ")&&strstr(line," ACK ")&&strstr(line,"sasl")) {
            got_sasl=1;if(!write_line(ssl,"AUTHENTICATE PLAIN\r\n"))break;
        } else if(got_sasl&&!strcmp(line,"AUTHENTICATE +")) {
            if(!authenticate(ssl,cfg))break;
        } else if(strstr(line," 903 ")) {
            authed=1;if(!write_line(ssl,"CAP END\r\n"))break;
        } else if(strstr(line," 904 ")||strstr(line," 905 ")||strstr(line," 906 ")||strstr(line," 907 ")) {
            break;
        } else if(authed&&!joined&&strstr(line," 001 ")) {
            if(!command(ssl,"JOIN",cfg->channel))break;
            joined=1;
        } else if(authed&&strstr(line," PRIVMSG ")) {
            if(!incoming(ssl,cfg,line))break;
        }
    }
done:
    if(ssl){SSL_shutdown(ssl);SSL_free(ssl);}
    if(ctx)SSL_CTX_free(ctx);
    close(fd);
}
static void *worker_main(void *unused) {
    irc_config_t cfg; (void)unused;
    if(!config_read(&cfg))return NULL;
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
    return result->tests_executed>=20 && !result->tests_failed ?
        STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host) {
    if(worker_started||!host||!host->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    host_api=host;stopping=0;
    if(pthread_create(&worker,NULL,worker_main,NULL)) {
        host_api=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
    }
    worker_started=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) {
    if(!worker_started)return STNLABZ_MODULE_ERR_INVALID_STATE;
    stopping=1;
    if(pthread_join(worker,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
    worker_started=0;host_api=NULL;
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
    .id="irc",.name="Libera.Chat IRC",
    .version_major=0,.version_minor=1,.version_patch=0,
    .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
    .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void) {
    return &descriptor;
}
/* [End AI:GPT-6] */
