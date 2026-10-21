/* Experimental localhost GRU adapter. No IRC delivery, no automatic training.
 * Output is an unverified language candidate, never factual evidence. */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "module.h"
#include "gru_bridge.h"
#define GENERATE_URL "http://127.0.0.1:8765/generate"
#define LEARN_URL "http://127.0.0.1:8765/learn"
#define BODY_MAX 16384
static const stnlabz_module_host_t *host;
typedef struct {char data[BODY_MAX+1];size_t used;} body_t;
static size_t receive(char *ptr,size_t size,size_t count,void *ctx){
 body_t *b=ctx;size_t n;
 if(size&&count>SIZE_MAX/size)return 0;
 n=size*count;
 if(n>BODY_MAX-b->used)return 0;
 memcpy(b->data+b->used,ptr,n);b->used+=n;b->data[b->used]=0;
 return n;
}
static int bounded(const char *p,size_t cap,int require){
 const char *end=p?memchr(p,0,cap):NULL;
 return end&&(!require||end!=p);
}
static int generate_valid(const digit_gru_generate_request_t *q){
 size_t n;
 if(!q||!bounded(q->prompt,sizeof q->prompt,1))return 0;
 n=strlen(q->prompt);
 return n<=1024&&q->length<=256&&
   (q->temperature==0||(q->temperature>=0.25&&q->temperature<=2.0))&&
   q->penalty>=0&&q->penalty<=2.0;
}
static int learn_valid(const digit_gru_learn_request_t *q){
 return q&&q->approved==1&&
  bounded(q->input,sizeof q->input,1)&&
  bounded(q->response,sizeof q->response,1)&&
  bounded(q->source,sizeof q->source,1);
}
static int send_json(const char *url,json_object *payload,
 const char *token,body_t *body,long *code){
 CURL *c;CURLcode result;struct curl_slist *headers=NULL;
 const char *encoded;
 if(!payload||!body||!code)return 0;
 encoded=json_object_to_json_string_ext(payload,JSON_C_TO_STRING_PLAIN);
 if(!encoded||!(c=curl_easy_init()))return 0;
 headers=curl_slist_append(headers,"Content-Type: application/json");
 if(token){
  size_t n=strlen(token);
  char header[256];
  if(n<1||n>190){curl_slist_free_all(headers);curl_easy_cleanup(c);return 0;}
  snprintf(header,sizeof header,"X-STN-Learn-Token: %s",token);
  headers=curl_slist_append(headers,header);
 }
 if(!headers){curl_easy_cleanup(c);return 0;}
 memset(body,0,sizeof *body);*code=0;
 curl_easy_setopt(c,CURLOPT_URL,url);
 curl_easy_setopt(c,CURLOPT_POST,1L);
 curl_easy_setopt(c,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(c,CURLOPT_POSTFIELDS,encoded);
 curl_easy_setopt(c,CURLOPT_POSTFIELDSIZE,(long)strlen(encoded));
 curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive);
 curl_easy_setopt(c,CURLOPT_WRITEDATA,body);
 curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,1L);
 curl_easy_setopt(c,CURLOPT_TIMEOUT,4L);
 curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(c,CURLOPT_PROXY,"");
 curl_easy_setopt(c,CURLOPT_NOPROXY,"*");
 result=curl_easy_perform(c);
 if(result==CURLE_OK)curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,code);
 curl_slist_free_all(headers);curl_easy_cleanup(c);
 return result==CURLE_OK;
}
static const char *string_field(json_object *obj,const char *key){
 json_object *value=NULL;
 if(!json_object_object_get_ex(obj,key,&value)||
    !json_object_is_type(value,json_type_string))return NULL;
 return json_object_get_string(value);
}
static int bool_field(json_object *obj,const char *key,int *value){
 json_object *v=NULL;
 if(!json_object_object_get_ex(obj,key,&v)||
    !json_object_is_type(v,json_type_boolean))return 0;
 *value=json_object_get_boolean(v);return 1;
}
static stnlabz_module_result_t generate(const void *req,size_t bytes,
 void *res,size_t cap,size_t *used,void *ctx){
 const digit_gru_generate_request_t *q=req;
 digit_gru_generate_result_t *out=res;
 json_object *payload=NULL,*parsed=NULL,*v=NULL;
 const char *text,*model;body_t body;long code=0;int experimental;
 (void)ctx;if(used)*used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(bytes!=sizeof *q||!generate_valid(q)||!out||cap<sizeof *out||!used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);
 payload=json_object_new_object();if(!payload)return STNLABZ_MODULE_ERR_INVALID_STATE;
 json_object_object_add(payload,"prompt",json_object_new_string(q->prompt));
 json_object_object_add(payload,"length",json_object_new_int(q->length?q->length:128));
 json_object_object_add(payload,"seed",json_object_new_uint64(q->seed?q->seed:913));
 json_object_object_add(payload,"temperature",json_object_new_double(q->temperature?q->temperature:0.65));
 json_object_object_add(payload,"penalty",json_object_new_double(q->penalty));
 if(!send_json(GENERATE_URL,payload,NULL,&body,&code)||code!=200)goto fail;
 parsed=json_tokener_parse(body.data);if(!parsed)goto fail;
 text=string_field(parsed,"text");model=string_field(parsed,"model");
 if(!text||strlen(text)>=sizeof out->text||!model||
    strcmp(model,"stn-labz-gru")||
    !json_object_object_get_ex(parsed,"generated_bytes",&v)||
    !json_object_is_type(v,json_type_int)||
    !bool_field(parsed,"experimental",&experimental))goto fail;
 if(json_object_get_int(v)<0||json_object_get_int(v)>256)goto fail;
 snprintf(out->text,sizeof out->text,"%s",text);
 out->generated_bytes=(uint32_t)json_object_get_int(v);
 out->experimental=(uint32_t)experimental;
 *used=sizeof *out;
 json_object_put(parsed);json_object_put(payload);return STNLABZ_MODULE_OK;
fail:
 if(parsed)json_object_put(parsed);
 json_object_put(payload);return STNLABZ_MODULE_ERR_INVALID_STATE;
}

static stnlabz_module_result_t learn(const void *req,size_t bytes,
 void *res,size_t cap,size_t *used,void *ctx){
 const digit_gru_learn_request_t *q=req;
 digit_gru_learn_result_t *out=res;
 const char *status;
 json_object *payload=NULL,*parsed=NULL;body_t body;long code=0;
 int accepted,trained;
 (void)ctx;if(used)*used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(bytes!=sizeof *q||!learn_valid(q)||!out||cap<sizeof *out||!used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* [AI:GPT-6 | 2026-10-21 UTC]
  * GRU /learn is bound to the operator-controlled loopback environment.
  * No token header required; preserve approved input/response/source. 
  * [End AI:GPT-6] */
 memset(out,0,sizeof *out);
 payload=json_object_new_object();if(!payload)return STNLABZ_MODULE_ERR_INVALID_STATE;
 json_object_object_add(payload,"input",json_object_new_string(q->input));
 json_object_object_add(payload,"response",json_object_new_string(q->response));
 json_object_object_add(payload,"source",json_object_new_string(q->source));
 json_object_object_add(payload,"approved",json_object_new_boolean(1));
 if(!send_json(LEARN_URL,payload,NULL,&body,&code)||code!=202)goto fail;
 parsed=json_tokener_parse(body.data);if(!parsed)goto fail;
 status=string_field(parsed,"status");
 if(!status||strlen(status)>=sizeof out->status||
    !bool_field(parsed,"accepted",&accepted)||
    !bool_field(parsed,"trained",&trained)||
    !accepted||trained||strcmp(status,"queued_for_review"))goto fail;
 out->accepted=1;out->trained=0;
 snprintf(out->status,sizeof out->status,"%s",status);
 *used=sizeof *out;
 json_object_put(parsed);json_object_put(payload);return STNLABZ_MODULE_OK;
fail:
 if(parsed)json_object_put(parsed);
 json_object_put(payload);return STNLABZ_MODULE_ERR_INVALID_STATE;
}
static void check(stnlabz_module_qualification_result_t *r,int ok,int negative){
 r->tests_executed++;if(ok)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 digit_gru_generate_request_t a={0};
 digit_gru_learn_request_t b={0};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 check(r,!generate_valid(&a),1);
 strcpy(a.prompt,"Hello");
 check(r,generate_valid(&a),0);
 a.length=256;check(r,generate_valid(&a),0);
 a.length=257;check(r,!generate_valid(&a),1);a.length=0;
 a.temperature=0.25;check(r,generate_valid(&a),0);
 a.temperature=2;check(r,generate_valid(&a),0);
 a.temperature=0.24;check(r,!generate_valid(&a),1);
 a.temperature=2.01;check(r,!generate_valid(&a),1);
 a.temperature=0.65;
 a.penalty=2;check(r,generate_valid(&a),0);
 a.penalty=2.1;check(r,!generate_valid(&a),1);
 a.penalty=-0.1;check(r,!generate_valid(&a),1);
 a.penalty=0;
 memset(a.prompt,'x',sizeof a.prompt);
 check(r,!generate_valid(&a),1);
 a.prompt[1024]=0;check(r,generate_valid(&a),0);
 a.prompt[0]=0;check(r,!generate_valid(&a),1);
 check(r,!generate_valid(NULL),1);
 check(r,!learn_valid(&b),1);
 strcpy(b.input,"Question");strcpy(b.response,"Reviewed answer");
 strcpy(b.source,"digit");
 check(r,!learn_valid(&b),1);
 b.approved=1;check(r,learn_valid(&b),0);
 b.approved=2;check(r,!learn_valid(&b),1);
 b.approved=1;b.source[0]=0;check(r,!learn_valid(&b),1);
 strcpy(b.source,"digit");b.response[0]=0;check(r,!learn_valid(&b),1);
 strcpy(b.response,"Reviewed answer");b.input[0]=0;
 check(r,!learn_valid(&b),1);
 check(r,!learn_valid(NULL),1);
 return r->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&!r->tests_failed&&
  r->negative_test_executed&&r->negative_test_passed?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->register_service||!api->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!api->register_service(DIGIT_GRU_GENERATE_SERVICE,generate,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 if(!api->register_service(DIGIT_GRU_LEARN_SERVICE,learn,NULL)){
  api->unregister_service(DIGIT_GRU_GENERATE_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=api;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_GRU_LEARN_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_GRU_GENERATE_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="gru_bridge",.name="Experimental STN GRU Bridge",
 .version_major=0,.version_minor=5,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
