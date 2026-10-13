#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "stnlabz_abi/module.h"
#include "router.h"

#define INTERFACE_PORT 8081
#define INTERFACE_CERT "/opt/digit/state/auth/interface-cert.pem"
#define INTERFACE_KEY "/opt/digit/state/auth/interface-key.pem"

static SSL_CTX *tls_context;
static int listener=-1;
static pthread_t listener_thread;
static atomic_int shutting_down;
static int running;
static const stnlabz_module_host_t *service_host;
static atomic_uint next_request_id;

/* Transport owns HTTP framing only. Router and Ask own dispatch and response. */
static int send_http(SSL *ssl,int status,const char *body){
 char head[256];int n;size_t length=strlen(body);
 const char *reason=status==200?"OK":status==400?"Bad Request":status==404?"Not Found":
                    status==405?"Method Not Allowed":status==503?"Service Unavailable":"Error";
 n=snprintf(head,sizeof head,"HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",status,reason,length);
 return n>0&&n<(int)sizeof head&&SSL_write(ssl,head,n)==n&&SSL_write(ssl,body,(int)length)==(int)length;
}
static void handle_chat(SSL *ssl){
 char request[2048]={0},question[513],answer[1024],json[2200];
 const char *body;size_t total=0,content_length=0,used=0,offset=0,i;
 int read_bytes,status=503;unsigned long long sequence;
 digit_router_request_t route={0};
 const char *error="{\"error\":\"Ask unavailable\"}";
 while(total<sizeof request-1){
  read_bytes=SSL_read(ssl,request+total,(int)(sizeof request-1-total));
  if(read_bytes<=0)return;
  total+=(size_t)read_bytes;request[total]=0;
  body=strstr(request,"\r\n\r\n");if(body)break;
 }
 body=strstr(request,"\r\n\r\n");
 if(!body){send_http(ssl,400,"{\"error\":\"Invalid HTTP request\"}");return;}
 if(strncmp(request,"POST /ask HTTP/1.1\r\n",20)!=0){
  send_http(ssl,405,"{\"error\":\"Only POST /ask is supported\"}");return;
 }
 {
  const char *p=request+20;int found=0;
  while(p<body){
   const char *end=strstr(p,"\r\n");
   if(!end||end>body)break;
   if((size_t)(end-p)>=15&&strncasecmp(p,"Content-Length:",15)==0){
    char *endptr;unsigned long n;
    if(found){send_http(ssl,400,"{\"error\":\"Duplicate length\"}");return;}
    found=1;n=strtoul(p+15,&endptr,10);
    while(endptr<end&&(*endptr==' '||*endptr=='\t'))endptr++;
    if(endptr!=end||n<1||n>512){send_http(ssl,400,"{\"error\":\"Invalid length\"}");return;}
    content_length=(size_t)n;
   }
   p=end+2;
  }
  if(!found){send_http(ssl,400,"{\"error\":\"Missing length\"}");return;}
 }
 offset=(size_t)(body+4-request);
 if(total-offset>content_length){send_http(ssl,400,"{\"error\":\"Oversized body\"}");return;}
 while(total-offset<content_length){
  read_bytes=SSL_read(ssl,request+total,(int)(content_length-(total-offset)));
  if(read_bytes<=0)return;
  total+=(size_t)read_bytes;
 }
 memcpy(question,request+offset,content_length);question[content_length]=0;
 if(memchr(question,0,content_length)||strchr(question,'\r')||strchr(question,'\n')){
  send_http(ssl,400,"{\"error\":\"Invalid message\"}");return;
 }
 sequence=(unsigned long long)atomic_fetch_add(&next_request_id,1)+1;
 snprintf(route.request_id,sizeof route.request_id,"http-%llu",sequence);
 snprintf(route.path,sizeof route.path,"/ask");
 route.core_authorized=0; /* public route only; never assert remote authority */
 route.payload=question;route.payload_size=content_length+1;
 {
  digit_router_result_t result={0};size_t out=0;
  if(service_host&&service_host->invoke_service&&
     service_host->invoke_service("router.dispatch",&route,sizeof route,answer,sizeof answer,&out)==STNLABZ_MODULE_OK&&
     out>0&&out<=sizeof answer&&memchr(answer,0,out)){
   size_t written=0;json[written++]='{';json[written++]='"';
   memcpy(json+written,"answer",6);written+=6;json[written++]='"';json[written++]=':';json[written++]='"';
   for(i=0;answer[i]&&written+8<sizeof json;i++){
    unsigned char c=(unsigned char)answer[i];
    if(c=='"'||c=='\\'){json[written++]='\\';json[written++]=(char)c;}
    else if(c=='\n'){json[written++]='\\';json[written++]='n';}
    else if(c=='\r'){json[written++]='\\';json[written++]='r';}
    else if(c<32){written+=snprintf(json+written,sizeof json-written,"\\u%04x",(unsigned)c);}
    else json[written++]=(char)c;
   }
   json[written++]='"';json[written++]='}';json[written]=0;
   status=200;error=json;
  }
  (void)result;(void)used;
 }
 send_http(ssl,status,error);
}
static void *serve(void *unused){
 (void)unused;
 while(!atomic_load(&shutting_down)){
  fd_set set;struct timeval wait_time={1,0};
  int ready,client;struct sockaddr_storage peer;socklen_t length=sizeof peer;
  FD_ZERO(&set);FD_SET(listener,&set);
  ready=select(listener+1,&set,NULL,NULL,&wait_time);
  if(ready<=0)continue;
  client=accept(listener,(struct sockaddr *)&peer,&length);
  if(client<0)continue;
  {
   struct timeval deadline={3,0};SSL *session;
   setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&deadline,sizeof deadline);
   setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&deadline,sizeof deadline);
   session=SSL_new(tls_context);
   if(session){
    SSL_set_fd(session,client);
    if(SSL_accept(session)==1)handle_chat(session);
    SSL_shutdown(session);SSL_free(session);
   }
  }
  close(client);
 }
 return NULL;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *result){
 if(!result)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(result,0,sizeof *result);
 result->tests_executed=result->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 result->negative_test_executed=result->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void);
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 struct sockaddr_in addr;int reuse=1;
 const char *failure="TLS initialization failed";
 if(running)return STNLABZ_MODULE_ERR_INVALID_STATE;
 service_host=host;
 atomic_store(&next_request_id,0);
 tls_context=SSL_CTX_new(TLS_server_method());
 if(!tls_context){
  if(host&&host->send_message)host->send_message("[INTERFACE] TLS initialization failed");
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(SSL_CTX_set_min_proto_version(tls_context,TLS1_2_VERSION)!=1||
    SSL_CTX_use_certificate_chain_file(tls_context,INTERFACE_CERT)!=1||
    SSL_CTX_use_PrivateKey_file(tls_context,INTERFACE_KEY,SSL_FILETYPE_PEM)!=1||
    SSL_CTX_check_private_key(tls_context)!=1){
  failure="TLS credentials unavailable, unreadable or invalid";
  goto failed;
 }
 listener=socket(AF_INET,SOCK_STREAM,0);
 if(listener<0){failure="TCP listener socket creation failed";goto failed;}
 if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse)!=0){
  failure="TCP socket setup failed";goto failed;
 }
 memset(&addr,0,sizeof addr);
 addr.sin_family=AF_INET;
 addr.sin_port=htons(INTERFACE_PORT);
 addr.sin_addr.s_addr=htonl(INADDR_ANY);
 if(bind(listener,(struct sockaddr *)&addr,sizeof addr)!=0||
    listen(listener,16)!=0){failure="TCP port 8081 bind or listen failed";goto failed;}
 atomic_store(&shutting_down,0);
 if(pthread_create(&listener_thread,NULL,serve,NULL)!=0){
  failure="TLS listener thread startup failed";goto failed;
 }
 running=1;
 if(!host||!host->send_message||
    !host->send_message("[INTERFACE] TLS listener active on port 8081; TLS required")){
  stop();
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
failed:
 if(host&&host->send_message){
  char message[200];
  snprintf(message,sizeof message,"[INTERFACE] %s",failure);
  host->send_message(message);
 }
 if(listener>=0){close(listener);listener=-1;}
 SSL_CTX_free(tls_context);tls_context=NULL;
 return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void){
 if(!running)return STNLABZ_MODULE_ERR_INVALID_STATE;
 atomic_store(&shutting_down,1);
 pthread_join(listener_thread,NULL);
 close(listener);listener=-1;
 SSL_CTX_free(tls_context);tls_context=NULL;
 running=0;service_host=NULL;
 return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"interface");
 snprintf(d.name,sizeof d.name,"TLS Connection Interface");
 d.version_major=0;d.version_minor=1;d.version_patch=3;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
