#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <sys/acl.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include "stnlabz_abi/module.h"
#include "digit_route.h"
#include "web_transport.h"

/* Local transport only; no TCP listener and no Core changes.
 * Protocol: request BE32 length + UTF-8 bytes; response U8 status +
 * BE32 length + bytes. Status 0 means Core replied; 1 means failure.
 * Wire text limits reflect the currently verified router service ABI. */
#define WEB_SOCKET_PATH "/run/digit-web/digit.sock"
#define WEB_MAX_REQUEST (DIGIT_ROUTE_TEXT_MAX - 1U)
#define WEB_MAX_REPLY (DIGIT_ROUTE_REPLY_MAX - 1U)
#define WEB_IO_TIMEOUT_SEC 5

static const stnlabz_module_host_t *host_api;
static pthread_t worker;
static int worker_running;
static atomic_int stopped;
static atomic_int listener_fd;
static uid_t permitted_uid;
static dev_t socket_dev;
static ino_t socket_ino;

static int parse_uid(const char *text,uid_t *uid)
{
 unsigned long n;
 char *end=NULL;
 if(!text||!*text||!uid)return 0;
 for(const char *p=text;*p;++p)if(*p<'0'||*p>'9')return 0;
 errno=0;
 n=strtoul(text,&end,10);
 if(errno||!end||*end||n==0UL||(unsigned long)(uid_t)n!=n)return 0;
 *uid=(uid_t)n;
 return 1;
}
static int validate_text(const char *text,size_t n)
{
 size_t i;
 if(!text||!n||n>WEB_MAX_REQUEST)return 0;
 for(i=0;i<n;++i){
  unsigned char c=(unsigned char)text[i];
  if(!c||c=='\r'||c=='\n'||c<0x20U||c==0x7fU)return 0;
 }
 return 1;
}
static int encode_request_length(size_t n,unsigned char header[4])
{
 uint32_t length;
 if(!header||!n||n>WEB_MAX_REQUEST)return 0;
 length=htonl((uint32_t)n);
 memcpy(header,&length,4U);
 return 1;
}
static int decode_request_length(const unsigned char header[4],size_t *n)
{
 uint32_t length;
 if(!header||!n)return 0;
 memcpy(&length,header,4U);
 length=ntohl(length);
 if(!length||length>WEB_MAX_REQUEST)return 0;
 *n=(size_t)length;
 return 1;
}
static int write_all(int fd,const void *source,size_t length)
{
 const unsigned char *data=source;
 size_t done=0U;
 while(done<length){
  ssize_t sent=send(fd,data+done,length-done,MSG_NOSIGNAL);
  if(sent<0&&errno==EINTR)continue;
  if(sent<=0)return 0;
  done+=(size_t)sent;
 }
 return 1;
}
static int read_all(int fd,void *dest,size_t length)
{
 unsigned char *data=dest;
 size_t done=0U;
 while(done<length){
  ssize_t got=recv(fd,data+done,length-done,0);
  if(got<0&&errno==EINTR)continue;
  if(got<=0)return 0;
  done+=(size_t)got;
 }
 return 1;
}
static int send_response(int fd,int success,const char *message)
{
 unsigned char header[5]={0};
 uint32_t length;
 size_t n;
 if(!message)return 0;
 n=strlen(message);
 if(n>WEB_MAX_REPLY)return 0;
 header[0]=success?0U:1U;
 length=htonl((uint32_t)n);
 memcpy(header+1U,&length,4U);
 return write_all(fd,header,sizeof header)&&
        (!n||write_all(fd,message,n));
}
static void process_client(int fd)
{
 struct ucred peer={0};
 socklen_t peer_size=sizeof peer;
 struct timeval timeout={.tv_sec=WEB_IO_TIMEOUT_SEC,.tv_usec=0};
 unsigned char header[4];
 digit_route_request_t request={0};
 digit_route_reply_t response={0};
 size_t length=0,used=0;
 stnlabz_module_result_t result;
 /* Both the Unix directory/socket mode AND the peer UID must authorize. */
 if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&peer_size)!=0||
    peer_size!=sizeof peer||peer.uid!=permitted_uid)return;
 (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof timeout);
 (void)setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof timeout);
 if(!read_all(fd,header,sizeof header)||
    !decode_request_length(header,&length)||
    !read_all(fd,request.text,length))return;
 if(!validate_text(request.text,length)){
  (void)send_response(fd,0,"Invalid request.");
  return;
 }
 request.text[length]=0;
 /* The trusted *local process* is identified; do not invent end-user auth. */
 snprintf(request.sender,sizeof request.sender,"digit-web");
 if(!host_api||!host_api->invoke_service){
  (void)send_response(fd,0,"Digit routing unavailable.");
  return;
 }
 result=host_api->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
          &request,sizeof request,&response,sizeof response,&used);
 if(result!=STNLABZ_MODULE_OK||used!=sizeof response||
    !response.replied||!memchr(response.text,0,sizeof response.text)||
    !response.text[0]){
  (void)send_response(fd,0,"Digit could not complete the request.");
  return;
 }
 (void)send_response(fd,1,response.text);
}
static void *listen_loop(void *ignored)
{
 (void)ignored;
 while(!atomic_load(&stopped)){
  struct pollfd pfd={.fd=atomic_load(&listener_fd),.events=POLLIN};
  int rc;
  if(pfd.fd<0)break;
  rc=poll(&pfd,1,250);
  if(rc<0&&errno==EINTR)continue;
  if(rc<0||rc==0)continue;
  if(pfd.revents&(POLLERR|POLLHUP|POLLNVAL))break;
  if(pfd.revents&POLLIN){
   int fd=accept(pfd.fd,NULL,NULL);
   if(fd<0)continue;
   process_client(fd);
   close(fd);
  }
 }
 return NULL;
}
static int verify_parent(void)
{
 struct stat st;
 /* Require a manually provisioned protected runtime directory. */
 if(lstat("/run/digit-web",&st)!=0||!S_ISDIR(st.st_mode)||
    st.st_uid!=geteuid()||(st.st_mode&0007U)!=0U||
    (st.st_mode&0020U)!=0U)return 0;
 return 1;
}
/* Replace inherited directory ACLs with the socket's intended access
 * policy. chmod(0660) changes an extended ACL mask, not its group entry. */
static int set_socket_access(void)
{
 acl_t acl=acl_from_text("u::rw-,g::rw-,o::---");
 int ok;
 if(!acl)return 0;
 ok=acl_set_file(WEB_SOCKET_PATH,ACL_TYPE_ACCESS,acl)==0;
 acl_free(acl);
 return ok;
}
static int make_listener(void)
{
 struct sockaddr_un address={0};
 struct stat st;
 int fd=-1;
 if(!verify_parent())return -1;
 /* Refuse an existing pathname, including a stale socket or symlink.
  * Operator must remove a stale socket after confirming ownership. */
 if(lstat(WEB_SOCKET_PATH,&st)==0||errno!=ENOENT)return -1;
 fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
 if(fd<0)return -1;
 address.sun_family=AF_UNIX;
 snprintf(address.sun_path,sizeof address.sun_path,"%s",WEB_SOCKET_PATH);
 if(bind(fd,(struct sockaddr *)&address,sizeof address)!=0)goto fail;
 if(!set_socket_access())goto fail_unlink;
 if(lstat(WEB_SOCKET_PATH,&st)!=0||!S_ISSOCK(st.st_mode))goto fail_unlink;
 socket_dev=st.st_dev;
 socket_ino=st.st_ino;
 if(listen(fd,8)!=0)goto fail_unlink;
 return fd;
fail_unlink:
 (void)unlink(WEB_SOCKET_PATH);
fail:
 close(fd);
 return -1;
}
static void unlink_owned_socket(void)
{
 struct stat st;
 if(!lstat(WEB_SOCKET_PATH,&st)&&S_ISSOCK(st.st_mode)&&
    st.st_uid==geteuid()&&st.st_dev==socket_dev&&st.st_ino==socket_ino)
  (void)unlink(WEB_SOCKET_PATH);
 socket_dev=0;
 socket_ino=0;
}
static int selftests(unsigned *executed,unsigned *passed,
                     int *negative_executed,int *negative_passed)
{
 unsigned e=0,p=0;
 int ne=0,np=0;
 uid_t uid=0;
 unsigned char header[4]={0};
 size_t count=0;
 #define CHECK(x) do{++e;if(x)++p;}while(0)
 CHECK(parse_uid("1001",&uid)&&uid==(uid_t)1001);
 CHECK(!parse_uid(NULL,&uid));
 CHECK(!parse_uid("0",&uid));
 CHECK(!parse_uid("10x",&uid));
 CHECK(!parse_uid("-12",&uid));
 CHECK(validate_text("Hello",5));
 CHECK(validate_text("research PHP 8.5",16));
 CHECK(!validate_text("",0));
 CHECK(!validate_text("hello\nthere",11));
 CHECK(!validate_text("bad\0message",11));
 CHECK(encode_request_length(2047U,header));
 CHECK(decode_request_length(header,&count)&&count==2047U);
 CHECK(!encode_request_length(2048U,header));
 CHECK(!decode_request_length((unsigned char[]){0,0,8,0},&count));
 CHECK(!decode_request_length((unsigned char[]){0,0,0,0},&count));
 CHECK(!strcmp(DIGIT_ROUTER_DISPATCH_SERVICE,"digit.router.dispatch"));
 #undef CHECK
 ne=1;
 np=!validate_text(NULL,1);
 if(executed)*executed=e;
 if(passed)*passed=p;
 if(negative_executed)*negative_executed=ne;
 if(negative_passed)*negative_passed=np;
 return e==p&&e>=STNLABZ_MODULE_MIN_TESTS&&np;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned e=0,p=0;
 int ne=0,np=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 int ok=selftests(&e,&p,&ne,&np);
 q->tests_executed=e;
 q->tests_passed=p;
 q->tests_failed=e-p;
 q->negative_test_executed=ne;
 q->negative_test_passed=np;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
 const char *configured=getenv("DIGIT_WEB_ALLOWED_UID");
 int fd;
 if(!h||!h->invoke_service||worker_running||
    !parse_uid(configured,&permitted_uid))
  return STNLABZ_MODULE_ERR_START_FAILED;
 atomic_store(&stopped,0);
 fd=make_listener();
 if(fd<0)return STNLABZ_MODULE_ERR_START_FAILED;
 host_api=h;
 atomic_store(&listener_fd,fd);
 if(pthread_create(&worker,NULL,listen_loop,NULL)!=0){
  atomic_store(&listener_fd,-1);
  close(fd);
  unlink_owned_socket();
  host_api=NULL;
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 worker_running=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 int fd;
 if(!worker_running)return STNLABZ_MODULE_OK;
 atomic_store(&stopped,1);
 /* Keep listen fd open until the worker exits to avoid fd-reuse races. */
 (void)pthread_join(worker,NULL);
 fd=atomic_exchange(&listener_fd,-1);
 if(fd>=0)close(fd);
 worker_running=0;
 host_api=NULL;
 unlink_owned_socket();
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "web","Digit Local Web Transport",1,0,0,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
