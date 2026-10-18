#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "irc.h"
#include "report.h"
#include "irc_render.h"
#define IRC_ENV_PATH "/opt/digit/state/auth/irc.env"
typedef struct {char server[256],port[16],nick[64],channel[128],account[128],password[256];} config_t;
static volatile sig_atomic_t stopping;
static void stop(int sig){(void)sig;stopping=1;}
static int putval(char *dst,size_t cap,const char *src){
 size_t n=strlen(src);if(n&&((src[0]=='"'&&src[n-1]=='"')||(src[0]=='\''&&src[n-1]=='\''))){src++;n-=2;}
 if(!n||n>=cap)return 0;
 memcpy(dst,src,n);dst[n]=0;return 1;
}
static int read_env(config_t *c){
 FILE *f;char line[1024];
 if(!c)return 0;memset(c,0,sizeof *c);
 f=fopen(IRC_ENV_PATH,"r");if(!f)return 0;
 while(fgets(line,sizeof line,f)){
  char *p=line,*eq,*end;
  while(isspace((unsigned char)*p))p++;
  if(!strncmp(p,"export ",7))p+=7;
  if(*p=='#'||!*p)continue;
  eq=strchr(p,'=');if(!eq)continue;*eq++=0;
  end=p+strlen(p);while(end>p&&isspace((unsigned char)end[-1]))*--end=0;
  end=eq+strlen(eq);while(end>eq&&isspace((unsigned char)end[-1]))*--end=0;
  if(!strcmp(p,"DIGIT_IRC_SERVER"))(void)putval(c->server,sizeof c->server,eq);
  else if(!strcmp(p,"DIGIT_IRC_PORT"))(void)putval(c->port,sizeof c->port,eq);
  else if(!strcmp(p,"DIGIT_IRC_NICK"))(void)putval(c->nick,sizeof c->nick,eq);
  else if(!strcmp(p,"DIGIT_IRC_CHANNEL"))(void)putval(c->channel,sizeof c->channel,eq);
  else if(!strcmp(p,"DIGIT_IRC_ACCOUNT"))(void)putval(c->account,sizeof c->account,eq);
  else if(!strcmp(p,"DIGIT_IRC_PASSWORD"))(void)putval(c->password,sizeof c->password,eq);
 }
 fclose(f);
 return c->server[0]&&c->port[0]&&c->nick[0]&&c->channel[0]=='#'&&c->account[0]&&c->password[0];
}
static int sendline(SSL *ssl,const char *line){
 size_t size=strlen(line),off=0;int n;
 while(off<size){n=SSL_write(ssl,line+off,(int)(size-off));if(n<=0)return 0;off+=(size_t)n;}
 return 1;
}
static int sendfmt(SSL *ssl,const char *cmd,const char *arg){
 char line[512];int n=snprintf(line,sizeof line,"%s %s\r\n",cmd,arg);
 return n>0&&(size_t)n<sizeof line&&sendline(ssl,line);
}
static int connect_tls(const config_t *c,SSL_CTX **ctx_out,SSL **ssl_out){
 struct addrinfo hints={0},*addresses=NULL,*a;int fd=-1;SSL_CTX *ctx=NULL;SSL *ssl=NULL;
 hints.ai_socktype=SOCK_STREAM;hints.ai_family=AF_UNSPEC;
 if(getaddrinfo(c->server,c->port,&hints,&addresses)!=0)return -1;
 for(a=addresses;a;a=a->ai_next){
  fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol);
  if(fd>=0&&connect(fd,a->ai_addr,a->ai_addrlen)==0)break;
  if(fd>=0)close(fd);fd=-1;
 }
 freeaddrinfo(addresses);if(fd<0)return -1;
 ctx=SSL_CTX_new(TLS_client_method());
 if(!ctx||SSL_CTX_set_default_verify_paths(ctx)!=1)goto fail;
 SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);
 ssl=SSL_new(ctx);
 if(!ssl||SSL_set_fd(ssl,fd)!=1||
    SSL_set_tlsext_host_name(ssl,c->server)!=1||
    SSL_set1_host(ssl,c->server)!=1||
    SSL_connect(ssl)!=1||SSL_get_verify_result(ssl)!=X509_V_OK)goto fail;
 *ctx_out=ctx;*ssl_out=ssl;return fd;
fail:
 if(ssl)SSL_free(ssl);if(ctx)SSL_CTX_free(ctx);close(fd);return -1;
}
static int sasl_payload(SSL *ssl,const config_t *c){
 unsigned char plain[512];char encoded[720],line[750];size_t a=strlen(c->account),p=strlen(c->password);
 int n;
 if(a*2+p+2>sizeof plain)return 0;
 plain[0]=0;memcpy(plain+1,c->account,a);plain[a+1]=0;memcpy(plain+a+2,c->password,p);
 n=EVP_EncodeBlock((unsigned char *)encoded,plain,(int)(a*2+p+2));
 OPENSSL_cleanse(plain,sizeof plain);
 if(n<0||n>400)return 0;
 snprintf(line,sizeof line,"AUTHENTICATE %s\r\n",encoded);
 OPENSSL_cleanse(encoded,sizeof encoded);
 return sendline(ssl,line);
}
static int handle_message(SSL *ssl,const config_t *c,char *line){
 char *bang,*space,*command,*target,*body,*sender;size_t i,n;
 char request[512],reply[512];
 digit_report_kind_t kind;
 if(!line||line[0]!=':')return 1;
 sender=line+1;bang=strchr(sender,'!');if(!bang)return 1;
 *bang=0;space=strchr(bang+1,' ');if(!space)return 1;
 command=space+1;
 if(strncmp(command,"PRIVMSG ",8))return 1;
 target=command+8;space=strchr(target,' ');if(!space)return 1;*space++=0;
 if(*space!=':')return 1;body=space+1;
 n=strlen(c->nick);
 if(!strcasecmp(target,c->nick)){
  snprintf(request,sizeof request,"%s",body);target=sender;
 }else{
  if(strcasecmp(target,c->channel))return 1;
  if(strncasecmp(body,c->nick,n)||!(body[n]==':'||body[n]==','||body[n]==' '))return 1;
  body+=n;if(*body==':'||*body==',')body++;
  while(*body==' ')body++;
  snprintf(request,sizeof request,"%s",body);
 }
 kind=digit_report_classify(request);
 if(kind==DIGIT_REPORT_UNKNOWN)return 1;
 for(i=0;i<digit_report_lines(kind);i++){
  char answer[385];
  if(!digit_report_line(kind,i,answer,sizeof answer))return 0;
  if(!digit_irc_render(reply,sizeof reply,target,answer)||!sendline(ssl,reply))return 0;
 }
 return 1;
}
static int irc_session(const config_t *c){
 SSL_CTX *ctx=NULL;SSL *ssl=NULL;int fd,got_sasl=0,authed=0,joined=0;
 char line[2048],nickline[128];size_t used=0;int n;
 fd=connect_tls(c,&ctx,&ssl);if(fd<0)return 0;
 if(!sendline(ssl,"CAP LS 302\r\n")||
    !sendfmt(ssl,"NICK",c->nick)||
    !sendfmt(ssl,"USER","digit 0 * :Digit"))goto done;
 while(!stopping){
  char ch; n=SSL_read(ssl,&ch,1);if(n<=0)break;
  if(ch=='\r')continue;
  if(ch!='\n'){if(used+1>=sizeof line){used=0;continue;}line[used++]=ch;continue;}
  line[used]=0;used=0;
  if(!strncmp(line,"PING :",6)){if(!sendfmt(ssl,"PONG",line+5))break;continue;}
  if(strstr(line," CAP ")&&strstr(line," LS ")&&strstr(line,"sasl")){
   if(!sendline(ssl,"CAP REQ :sasl\r\n"))break;
  }else if(strstr(line," CAP ")&&strstr(line," ACK ")&&strstr(line,"sasl")){
   got_sasl=1;if(!sendline(ssl,"AUTHENTICATE PLAIN\r\n"))break;
  }else if(got_sasl&&!strcmp(line,"AUTHENTICATE +")){
   if(!sasl_payload(ssl,c))break;
  }else if(strstr(line," 903 ")){
   authed=1;if(!sendline(ssl,"CAP END\r\n"))break;
  }else if(strstr(line," 904 ")||strstr(line," 905 ")||strstr(line," 906 ")||strstr(line," 907 "))break;
  else if(authed&&!joined&&strstr(line," 001 ")){
   snprintf(nickline,sizeof nickline,"%s",c->channel);
   if(!sendfmt(ssl,"JOIN",nickline))break;joined=1;
  }else if(authed&&strstr(line," PRIVMSG ")){
   if(!handle_message(ssl,c,line))break;
  }
 }
done:
 if(ssl){SSL_shutdown(ssl);SSL_free(ssl);}
 if(ctx)SSL_CTX_free(ctx);close(fd);
 return authed&&joined;
}
int digit_irc_run(void){
 config_t c;
 struct sigaction action={0};
 if(!read_env(&c)){fputs("Digit IRC: configuration unavailable or incomplete\n",stderr);return 1;}
 if(strcmp(c.server,"irc.libera.chat")||strcasecmp(c.channel,"#stn-labz")){
  fputs("Digit IRC: expected Libera.Chat #stn-labz configuration\n",stderr);return 1;
 }
 action.sa_handler=stop;sigemptyset(&action.sa_mask);
 (void)sigaction(SIGINT,&action,NULL);
 (void)sigaction(SIGTERM,&action,NULL);
 while(!stopping){(void)irc_session(&c);if(!stopping)sleep(10);}
 OPENSSL_cleanse(&c,sizeof c);
 return 0;
}
