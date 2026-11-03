#define _GNU_SOURCE
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../src/web_module.c"

static int checks,passed;
static unsigned routed;
#define CHECK(x) do {++checks;if(x)++passed;else fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}while(0)

static stnlabz_module_result_t test_dispatch(const char *name,
 const void *input,size_t input_len,void *output,size_t output_len,
 size_t *used)
{
 const digit_route_request_t *request=input;
 digit_route_reply_t *reply=output;
 ++routed;
 if(strcmp(name,DIGIT_ROUTER_DISPATCH_SERVICE)||!request||
    input_len!=sizeof *request||!reply||output_len<sizeof *reply)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(reply,0,sizeof *reply);
 if(!strcmp(request->text,"Hello")){
  strcpy(reply->text,"Hello!");
  reply->replied=1;
  if(used)*used=sizeof *reply;
  return STNLABZ_MODULE_OK;
 }
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static void verify_roundtrip(const char *message,int expected_success,
                             const char *expected_reply)
{
 int fds[2]={-1,-1};
 unsigned char request_header[4],response_header[5];
 size_t reply_size;
 uint32_t length;
 char text[DIGIT_ROUTE_REPLY_MAX];
 CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0);
 if(fds[0]<0||fds[1]<0)return;
 permitted_uid=geteuid();
 CHECK(encode_request_length(strlen(message),request_header));
 CHECK(write_all(fds[0],request_header,sizeof request_header));
 CHECK(write_all(fds[0],message,strlen(message)));
 process_client(fds[1]);
 CHECK(read_all(fds[0],response_header,sizeof response_header));
 CHECK(response_header[0]==(expected_success?0U:1U));
 memcpy(&length,response_header+1,4U);
 reply_size=(size_t)ntohl(length);
 CHECK(reply_size<DIGIT_ROUTE_REPLY_MAX);
 if(reply_size<DIGIT_ROUTE_REPLY_MAX){
  CHECK(read_all(fds[0],text,reply_size));
  text[reply_size]=0;
  CHECK(!strcmp(text,expected_reply));
 }
 close(fds[0]);
 close(fds[1]);
}
int main(void)
{
 stnlabz_module_qualification_result_t q={0};
 stnlabz_module_host_t mock={.invoke_service=test_dispatch};
 unsigned char header[4]={0};
 size_t size=0;
 uid_t uid=0;
 CHECK(qualify(&q)==STNLABZ_MODULE_OK);
 CHECK(q.tests_executed>=STNLABZ_MODULE_MIN_TESTS);
 CHECK(q.tests_executed==q.tests_passed);
 CHECK(q.negative_test_executed==1&&q.negative_test_passed==1);
 CHECK(qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(!strcmp(stnlabz_module_get_descriptor()->id,"web"));
 CHECK(stnlabz_module_get_descriptor()->version_major==1);
 CHECK(stnlabz_module_get_descriptor()->version_minor==0);
 CHECK(stnlabz_module_get_descriptor()->version_patch==0);
 CHECK(start(NULL)==STNLABZ_MODULE_ERR_START_FAILED);
 CHECK(stop()==STNLABZ_MODULE_OK);
 CHECK(parse_uid("1234",&uid)&&uid==(uid_t)1234);
 CHECK(!parse_uid("1234x",&uid));
 CHECK(encode_request_length(128,header));
 CHECK(decode_request_length(header,&size)&&size==128U);
 CHECK(validate_text("/github poemei/digit tree",25));
 CHECK(!validate_text("/github\ntree",12));
 host_api=&mock;
 verify_roundtrip("Hello",1,"Hello!");
 verify_roundtrip("not found",0,"Digit could not complete the request.");
 CHECK(routed==2U);
 host_api=NULL;
 printf("Web transport: %d/%d checks passed\n",passed,checks);
 return checks==passed?0:1;
}
