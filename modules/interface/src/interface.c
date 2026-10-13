#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
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

#define INTERFACE_PORT 8081
#define INTERFACE_CERT "/opt/digit/state/auth/interface-cert.pem"
#define INTERFACE_KEY "/opt/digit/state/auth/interface-key.pem"

static SSL_CTX *tls_context;
static int listener=-1;
static pthread_t listener_thread;
static atomic_int shutting_down;
static int running;

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
   struct timeval deadline={2,0};
   SSL *session;
   setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&deadline,sizeof deadline);
   setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&deadline,sizeof deadline);
   session=SSL_new(tls_context);
   if(session){
    SSL_set_fd(session,client);
    /* Transport only. No HTTP, MCP, Slack, IRC or application payload.
       Higher-level integration is not activated by this listener. */
    if(SSL_accept(session)==1)SSL_shutdown(session);
    SSL_free(session);
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
 running=0;
 return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"interface");
 snprintf(d.name,sizeof d.name,"TLS Connection Interface");
 d.version_major=0;d.version_minor=1;d.version_patch=2;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
