#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "rictus_llm.h"
#include "ollama_internal.h"

#define OLLAMA_VERSION_MAJOR 1U
#define OLLAMA_VERSION_MINOR 0U
#define OLLAMA_VERSION_PATCH 2U

typedef struct { char *data; size_t used; size_t capacity; } buffer_t;
static const stnlabz_module_host_t *owner;
static int service_registered;

static size_t write_cb(void *contents,size_t size,size_t nmemb,void *ctx)
{
 buffer_t *b=ctx; size_t bytes,need; char *next;
 if(!b||!contents||!size||!nmemb||nmemb>SIZE_MAX/size)return 0;
 bytes=size*nmemb;
 if(b->used>RICTUS_OLLAMA_HTTP_MAX||bytes>RICTUS_OLLAMA_HTTP_MAX-b->used)return 0;
 need=b->used+bytes+1U;
 if(need>b->capacity){
  size_t cap=b->capacity?b->capacity:4096U;
  while(cap<need){ if(cap>=RICTUS_OLLAMA_HTTP_MAX/2U){cap=RICTUS_OLLAMA_HTTP_MAX+1U;break;} cap*=2U; }
  if(cap>RICTUS_OLLAMA_HTTP_MAX+1U)return 0;
  next=realloc(b->data,cap); if(!next)return 0; b->data=next;b->capacity=cap;
 }
 memcpy(b->data+b->used,contents,bytes);b->used+=bytes;b->data[b->used]=0;return bytes;
}

static stnlabz_module_result_t generate(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *used,void *ctx)
{
 const rictus_llm_request_t *in=request; rictus_llm_response_t *out=response;
 CURL *curl=NULL; struct curl_slist *headers=NULL; CURLcode rc; long status=0;
 char body[4096]; buffer_t received={0}; (void)ctx;
 if(used)*used=0;
 if(!in||request_size!=sizeof(*in)||!out||response_size<sizeof(*out)||
    !memchr(in->prompt,0,sizeof in->prompt)||!in->prompt[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof(*out));
 if(!rictus_ollama_build_request(in->prompt,body,sizeof body))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 curl=curl_easy_init(); if(!curl)return STNLABZ_MODULE_ERR_START_FAILED;
 headers=curl_slist_append(headers,"Content-Type: application/json");
 curl_easy_setopt(curl,CURLOPT_URL,RICTUS_OLLAMA_ENDPOINT);
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,body);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(body));
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_cb);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&received);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,1000L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,30000L);
 rc=curl_easy_perform(curl);
 if(rc==CURLE_OK)(void)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 curl_slist_free_all(headers);curl_easy_cleanup(curl);
 if(rc!=CURLE_OK||status!=200L||!received.data){
  fprintf(stderr,"[OLLAMA] request failed curl=%d http=%ld\n",(int)rc,status);
  free(received.data); return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(!rictus_ollama_extract_response(received.data,out->text,sizeof out->text)){
  fprintf(stderr,"[OLLAMA] response parse failed bytes=%zu\n",received.used);
  free(received.data); return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 fprintf(stderr,"[OLLAMA] generation complete bytes=%zu\n",received.used);
 free(received.data);
 if(used)*used=sizeof(*out);
 return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned int e=0,p=0;int ne=0,np=0;int ok;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));
 ok=rictus_ollama_run_selftests(&e,&p,&ne,&np);
 q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
 q->negative_test_executed=ne;q->negative_test_passed=np;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}

static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
 if(!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(RICTUS_LLM_GENERATE_SERVICE,generate,NULL)){
  curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 owner=h;service_registered=1;
 fprintf(stderr,"[OLLAMA] service active model=%s endpoint=%s\n",RICTUS_OLLAMA_MODEL,RICTUS_OLLAMA_ENDPOINT);
 return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t stop(void)
{
 if(service_registered&&owner)
  (void)owner->unregister_service(RICTUS_LLM_GENERATE_SERVICE,NULL);
 service_registered=0;owner=NULL;curl_global_cleanup();
 return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t descriptor={
 "ollama","Rictus Ollama LLM",OLLAMA_VERSION_MAJOR,OLLAMA_VERSION_MINOR,
 OLLAMA_VERSION_PATCH,STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
