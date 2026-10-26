/* [AI:GPT-6 | 2026-10-26 02:00:00 UTC]
 * Local operator-authorized language harness. SQLite3 and Reasoning own data
 * and evidence; output is an unverified candidate, never database authority.
 */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ollama.h"
#define ENDPOINT "http://127.0.0.1:11434/api/generate"
#define BODY_MAX 16384
typedef struct {char text[BODY_MAX+1];size_t used;} body_t;
static int active;
static const stnlabz_module_host_t *api;
static int field_ok(const char *s,size_t cap,int nonempty){
 const char *e=s?memchr(s,0,cap):NULL;size_t i;
 if(!e||(nonempty&&e==s))return 0;
 for(i=0;s[i];i++)if(((unsigned char)s[i]<32&&s[i]!='\n'&&s[i]!='\t')||
  (unsigned char)s[i]==127)return 0;
 return 1;
}
static int valid(const digit_ollama_request_t *r){
 return r&&field_ok(r->question,sizeof r->question,1)&&
 field_ok(r->evidence,sizeof r->evidence,0)&&r->max_tokens<=256&&
 r->evidence_required<=1&&(!r->evidence_required||r->evidence[0]);
}
static size_t capture(char *p,size_t n,size_t count,void *ctx){
 body_t *b=ctx;size_t size;
 if(n&&count>SIZE_MAX/n)return 0;
 size=n*count;if(!b||size>BODY_MAX-b->used)return 0;
 memcpy(b->text+b->used,p,size);b->used+=size;b->text[b->used]=0;
 return size;
}
static int parse_result(const char *json,digit_ollama_result_t *out){
 struct json_object *root=NULL,*v=NULL;const char *t,*model;int ok=0;
 if(!json||!out)return 0;
 root=json_tokener_parse(json);
 if(!root||!json_object_is_type(root,json_type_object))goto done;
 if(!json_object_object_get_ex(root,"model",&v)||
  !json_object_is_type(v,json_type_string))goto done;
 model=json_object_get_string(v);
 if(strcmp(model,DIGIT_OLLAMA_MODEL))goto done;
 if(!json_object_object_get_ex(root,"response",&v)||
  !json_object_is_type(v,json_type_string))goto done;
 t=json_object_get_string(v);
 if(!t||!*t||strlen(t)>=sizeof out->text)goto done;
 if(!json_object_object_get_ex(root,"done",&v)||
  !json_object_is_type(v,json_type_boolean)||!json_object_get_boolean(v))goto done;
 memset(out,0,sizeof *out);strcpy(out->text,t);
 out->generated_bytes=(uint32_t)strlen(t);out->unverified=1;ok=1;
done:
 if(root)json_object_put(root);
 return ok;
}
static stnlabz_module_result_t generate(const void *req,size_t size,
 void *response,size_t capacity,size_t *used,void *context){
 const digit_ollama_request_t *in=req;
 digit_ollama_result_t *out=response;body_t body={0};
 struct json_object *j=NULL,*opts=NULL;struct curl_slist *headers=NULL;
 CURL *curl=NULL;CURLcode code;long status=0;const char *data;
 char prompt[2048];int length,success=0;
 (void)context;
 if(used)*used=0;
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(size!=sizeof *in||!valid(in)||!response||
  capacity<sizeof *out||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);
 length=snprintf(prompt,sizeof prompt,
  /* [AI:GPT-6 | 2026-10-26] Conversational voice follows Rictus's
   * direct LLM service pattern; generation cannot change Digit's records. */
  "You are Digit's conversational voice, speaking directly to the person "
  "addressing Digit. Reply naturally, briefly and relevantly. "
  "Answer ordinary questions conversationally. Do not invent claims about "
  "Digit's identity, operational state, capabilities, actions or memory. "
  "Do not assert an unverified operational fact as verified. "
  "When trustworthy evidence is provided, use that evidence faithfully. "
  "If unsure about a factual answer, say so naturally. "
  "If someone asks for silence, do not produce an argumentative refusal.\n"
  "Message: %s\nVerified evidence: %s\nDigit:",in->question,
  in->evidence_required?in->evidence:"(none provided)");
 if(length<0||(size_t)length>=sizeof prompt)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 j=json_object_new_object();opts=json_object_new_object();
 if(!j||!opts)goto done;
 json_object_object_add(j,"model",json_object_new_string(DIGIT_OLLAMA_MODEL));
 json_object_object_add(j,"prompt",json_object_new_string(prompt));
 json_object_object_add(j,"stream",json_object_new_boolean(0));
 json_object_object_add(opts,"num_predict",json_object_new_int(in->max_tokens?in->max_tokens:128));
 json_object_object_add(j,"options",opts);opts=NULL;
 data=json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN);
 if(!data||(curl=curl_easy_init())==NULL)goto done;
 headers=curl_slist_append(headers,"Content-Type: application/json");
 if(!headers)goto done;
 curl_easy_setopt(curl,CURLOPT_URL,ENDPOINT);
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(data));
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,capture);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&body);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,2L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,45L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_NOPROXY,"*");
 code=curl_easy_perform(curl);
 if(code==CURLE_OK){
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
  if(status==200)success=parse_result(body.text,out);
 }
done:
 curl_slist_free_all(headers);if(curl)curl_easy_cleanup(curl);
 if(opts)json_object_put(opts);
 if(j)json_object_put(j);
 if(!success)return STNLABZ_MODULE_ERR_INVALID_STATE;
 *used=sizeof *out;return STNLABZ_MODULE_OK;
}
static void check(stnlabz_module_qualification_result_t *q,int ok,int neg){
 q->tests_executed++;if(ok)q->tests_passed++;else q->tests_failed++;
 if(neg){q->negative_test_executed=1;if(ok)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_ollama_request_t r={0};digit_ollama_result_t out={0};
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 check(q,!valid(NULL),1);check(q,!valid(&r),1);
 strcpy(r.question,"Hello");check(q,valid(&r),0);
 r.max_tokens=256;check(q,valid(&r),0);
 r.max_tokens=257;check(q,!valid(&r),1);r.max_tokens=0;
 r.evidence_required=1;check(q,!valid(&r),1);
 strcpy(r.evidence,"Operator-verified Corpus excerpt.");
 check(q,valid(&r),0);
 r.evidence_required=2;check(q,!valid(&r),1);
 r.evidence_required=1;check(q,valid(&r),0);
 r.question[0]=0;check(q,!valid(&r),1);
 strcpy(r.question,"Hello");check(q,valid(&r),0);
 r.question[1]='\r';check(q,!valid(&r),1);
 r.question[1]=1;check(q,!valid(&r),1);
 strcpy(r.question,"Hello");
 r.evidence[0]=127;check(q,!valid(&r),1);
 strcpy(r.evidence,"Evidence.");check(q,valid(&r),0);
 memset(r.question,'a',sizeof r.question);check(q,!valid(&r),1);
 r.question[sizeof r.question-1]=0;check(q,valid(&r),0);
 check(q,!parse_result(NULL,&out),1);
 check(q,!parse_result("invalid",&out),1);
 check(q,!parse_result("{}",&out),1);
 check(q,!parse_result("{\"model\":\"other\",\"response\":\"hi\",\"done\":true}",&out),1);
 check(q,!parse_result("{\"model\":\"llama3.2:1b\",\"response\":\"\",\"done\":true}",&out),1);
 check(q,!parse_result("{\"model\":\"llama3.2:1b\",\"response\":\"ok\",\"done\":false}",&out),1);
 check(q,parse_result("{\"model\":\"llama3.2:1b\",\"response\":\"ok\",\"done\":true}",&out),0);
 check(q,out.unverified==1,0);
 check(q,out.generated_bytes==2&&!strcmp(out.text,"ok"),0);
 return q->tests_failed||q->tests_executed<STNLABZ_MODULE_MIN_TESTS||
 !q->negative_test_executed||!q->negative_test_passed?
 STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 if(active||!host||!host->register_service||!host->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_service(DIGIT_OLLAMA_GENERATE_SERVICE,generate,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 api=host;active=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!active||!api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!api->unregister_service(DIGIT_OLLAMA_GENERATE_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 active=0;api=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="ollama",.name="Temporary Ollama Language Harness",
 .version_major=0,.version_minor=1,.version_patch=2,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
/* [End AI:GPT-6] */
