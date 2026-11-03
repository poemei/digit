#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "stnlabz_abi/module.h"
#include "digit_route.h"
#include "web_transport.h"

/* Like IRC, this transport is a worker which calls Digit's existing
 * transport-neutral router. Unlike the abandoned 1.0.0 implementation,
 * it never binds, listens, or creates a socket/port. */
#define WEB_DEFAULT_ENDPOINT "http://127.0.0.1/chat/transport"
#define WEB_HTTP_LIMIT 4096U
#define WEB_MAX_ID 64U
#define WEB_WAIT_NS 250000000L

typedef struct {char bytes[WEB_HTTP_LIMIT+1U];size_t used;} web_buffer_t;
typedef struct {char id[WEB_MAX_ID+1U];char text[DIGIT_ROUTE_TEXT_MAX];} web_job_t;
static pthread_t worker;
static atomic_int stop_requested;
static int worker_started;
static const stnlabz_module_host_t *module_host;
static char endpoint[512];

static int endpoint_allowed(const char *url)
{
 const char *p;
 size_t length;
 if(!url)return 0;
 length=strlen(url);
 if(length<strlen("http://127.0.0.1/")||length>=sizeof endpoint)return 0;
 if(strncmp(url,"http://127.0.0.1/",17)!=0)return 0;
 /* No credentials, query strings, redirects, alternate hosts or ports. */
 for(p=url+17;*p;++p){
  unsigned char c=(unsigned char)*p;
  if(c<=0x20U||c>=0x7fU||c=='?'||c=='#'||c=='@'||c=='\\')return 0;
 }
 return !strncmp(url+17,"chat/transport",14)&&
        (url[31]=='\0'||url[31]=='/');
}
static int valid_message(const char *text,size_t size)
{
 size_t i;
 if(!text||!size||size>=DIGIT_ROUTE_TEXT_MAX)return 0;
 for(i=0;i<size;++i){
  unsigned char c=(unsigned char)text[i];
  if(c==0||c<0x20U||c==0x7fU)return 0;
 }
 return 1;
}
static int valid_id(const char *id)
{
 size_t i,length;
 if(!id)return 0;
 length=strlen(id);
 if(!length||length>WEB_MAX_ID)return 0;
 for(i=0;i<length;++i){
  char c=id[i];
  if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
       (c>='0'&&c<='9')||c=='-'||c=='_'))return 0;
 }
 return 1;
}
static int parse_job(const char *body,web_job_t *job)
{
 struct json_object *root=NULL,*id_obj=NULL,*text_obj=NULL;
 const char *id,*message;
 size_t len;
 int ok=0;
 if(!body||!job)return 0;
 memset(job,0,sizeof *job);
 root=json_tokener_parse(body);
 if(!root||!json_object_is_type(root,json_type_object))goto done;
 if(!json_object_object_get_ex(root,"id",&id_obj)||
    !json_object_is_type(id_obj,json_type_string)||
    !json_object_object_get_ex(root,"message",&text_obj)||
    !json_object_is_type(text_obj,json_type_string))goto done;
 id=json_object_get_string(id_obj);
 message=json_object_get_string(text_obj);
 len=(size_t)json_object_get_string_len(text_obj);
 if(!valid_id(id)||!valid_message(message,len)||
    strlen(message)!=len)goto done;
 memcpy(job->id,id,strlen(id)+1U);
 memcpy(job->text,message,len+1U);
 ok=1;
done:
 if(root)json_object_put(root);
 return ok;
}
static int build_reply(const char *id,int success,const char *message,
                       char *out,size_t capacity)
{
 struct json_object *root=NULL;
 const char *text;
 size_t length;
 int ok=0;
 if(!valid_id(id)||!message||!out||!capacity)return 0;
 length=strlen(message);
 if(length>=DIGIT_ROUTE_REPLY_MAX)return 0;
 root=json_object_new_object();
 if(!root)return 0;
 json_object_object_add(root,"type",json_object_new_string("reply"));
 json_object_object_add(root,"id",json_object_new_string(id));
 json_object_object_add(root,"status",json_object_new_string(success?"ok":"error"));
 json_object_object_add(root,"message",json_object_new_string(message));
 text=json_object_to_json_string_ext(root,JSON_C_TO_STRING_PLAIN);
 if(text&&strlen(text)<capacity){strcpy(out,text);ok=1;}
 json_object_put(root);
 return ok;
}
static size_t receive_bytes(char *ptr,size_t size,size_t n,void *context)
{
 web_buffer_t *buffer=context;
 size_t length;
 if(!buffer||!ptr||size==0||n>SIZE_MAX/size)return 0;
 length=size*n;
 if(length>WEB_HTTP_LIMIT-buffer->used)return 0;
 memcpy(buffer->bytes+buffer->used,ptr,length);
 buffer->used+=length;
 buffer->bytes[buffer->used]=0;
 return length;
}
static int exchange(CURL *handle,const char *payload,web_buffer_t *buffer,
                    long *http_status)
{
 struct curl_slist *headers=NULL;
 CURLcode rc;
 if(!handle||!payload||!buffer||!http_status)return 0;
 memset(buffer,0,sizeof *buffer);
 *http_status=0;
 headers=curl_slist_append(NULL,"Content-Type: application/json");
 if(!headers)return 0;
 curl_easy_setopt(handle,CURLOPT_URL,endpoint);
 curl_easy_setopt(handle,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(handle,CURLOPT_POST,1L);
 curl_easy_setopt(handle,CURLOPT_POSTFIELDS,payload);
 curl_easy_setopt(handle,CURLOPT_POSTFIELDSIZE,(long)strlen(payload));
 curl_easy_setopt(handle,CURLOPT_WRITEFUNCTION,receive_bytes);
 curl_easy_setopt(handle,CURLOPT_WRITEDATA,buffer);
 curl_easy_setopt(handle,CURLOPT_CONNECTTIMEOUT_MS,1000L);
 curl_easy_setopt(handle,CURLOPT_TIMEOUT_MS,5000L);
 curl_easy_setopt(handle,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(handle,CURLOPT_PROXY,"");
 curl_easy_setopt(handle,CURLOPT_IPRESOLVE,CURL_IPRESOLVE_V4);
 rc=curl_easy_perform(handle);
 if(rc==CURLE_OK)(void)curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,http_status);
 curl_slist_free_all(headers);
 curl_easy_setopt(handle,CURLOPT_HTTPHEADER,NULL);
 return rc==CURLE_OK;
}
static int dispatch(const web_job_t *job,char *reply)
{
 digit_route_request_t request={0};
 digit_route_reply_t result={0};
 size_t used=0;
 stnlabz_module_result_t rc;
 if(!job||!reply||!module_host||!module_host->invoke_service)return 0;
 strcpy(request.sender,"digit-web");
 strcpy(request.text,job->text);
 rc=module_host->invoke_service(DIGIT_ROUTER_DISPATCH_SERVICE,
     &request,sizeof request,&result,sizeof result,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof result||!result.replied||
    !memchr(result.text,0,sizeof result.text)||!result.text[0])return 0;
 strcpy(reply,result.text);
 return 1;
}
static void sleep_short(void)
{
 struct timespec delay={.tv_sec=0,.tv_nsec=WEB_WAIT_NS};
 while(!atomic_load(&stop_requested)&&nanosleep(&delay,&delay)!=0){}
}
static void *run_transport(void *ignored)
{
 CURL *handle=curl_easy_init();
 (void)ignored;
 if(!handle)return NULL;
 while(!atomic_load(&stop_requested)){
  web_buffer_t incoming={0},confirmation={0};
  web_job_t job={0};
  char reply[DIGIT_ROUTE_REPLY_MAX]={0};
  char payload[1400];
  long status=0,ack=0;
  int success;
  if(!exchange(handle,"{\"type\":\"poll\"}",&incoming,&status)||
     status!=200L||!parse_job(incoming.bytes,&job)){
   sleep_short();
   continue;
  }
  success=dispatch(&job,reply);
  if(!success)strcpy(reply,"Digit could not complete the request.");
  if(build_reply(job.id,success,reply,payload,sizeof payload)){
   if(!exchange(handle,payload,&confirmation,&ack)||ack!=200L){
    fprintf(stderr,"[WEB] reply delivery failed; request id=%s\n",job.id);
   }
  }
  sleep_short();
 }
 curl_easy_cleanup(handle);
 return NULL;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned e=0,p=0;
 web_job_t job={0};
 char out[1400];
 #define TEST(x) do{++e;if(x)++p;}while(0)
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 TEST(endpoint_allowed(WEB_DEFAULT_ENDPOINT));
 TEST(!endpoint_allowed("https://example.com/chat/transport"));
 TEST(!endpoint_allowed("http://127.0.0.1:8000/chat/transport"));
 TEST(!endpoint_allowed("http://127.0.0.1/other"));
 TEST(!endpoint_allowed("http://127.0.0.1/chat/transport?x=1"));
 TEST(valid_id("req_01-a"));
 TEST(!valid_id("../bad"));
 TEST(valid_message("Hello",5));
 TEST(!valid_message("Hi\nWorld",8));
 TEST(parse_job("{\"id\":\"r1\",\"message\":\"Hello\"}",&job));
 TEST(!strcmp(job.id,"r1")&&!strcmp(job.text,"Hello"));
 TEST(!parse_job("{\"id\":\"../bad\",\"message\":\"Hello\"}",&job));
 TEST(!parse_job("{\"id\":\"r1\",\"message\":\"Hi\\nWorld\"}",&job));
 TEST(!parse_job("{\"id\":1,\"message\":\"Hello\"}",&job));
 TEST(build_reply("r1",1,"Hello!",out,sizeof out));
 TEST(strstr(out,"\"status\":\"ok\"")!=NULL);
 #undef TEST
 q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
 q->negative_test_executed=1;q->negative_test_passed=e==p;
 return e==p?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
 const char *configured=getenv("DIGIT_WEB_TRANSPORT_URL");
 if(!host||!host->invoke_service||worker_started)return STNLABZ_MODULE_ERR_START_FAILED;
 if(!configured||!*configured)configured=WEB_DEFAULT_ENDPOINT;
 if(!endpoint_allowed(configured))return STNLABZ_MODULE_ERR_START_FAILED;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return STNLABZ_MODULE_ERR_START_FAILED;
 strcpy(endpoint,configured);
 module_host=host;
 atomic_store(&stop_requested,0);
 if(pthread_create(&worker,NULL,run_transport,NULL)!=0){
  module_host=NULL;
  curl_global_cleanup();
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 worker_started=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(!worker_started)return STNLABZ_MODULE_OK;
 atomic_store(&stop_requested,1);
 if(pthread_join(worker,NULL)!=0)return STNLABZ_MODULE_ERR_STOP_FAILED;
 worker_started=0;
 module_host=NULL;
 curl_global_cleanup();
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "web","Digit HTTP Web Transport",1,0,1,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
