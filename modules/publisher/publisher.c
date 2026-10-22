/* Digit publisher v0.1.0 - ABI 1.5. Authorized Markdown submission only.
 * Site authentication contract is configured explicitly, never inferred.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"
#define CONFIG "/opt/digit/auth/publisher/publisher.conf"
#define SERVICE "digit.publisher.publish"
#define MAX_BODY 16384
typedef struct {
 char site[32],kind[32],category[64],title[256],markdown[MAX_BODY];
} publish_request_t;
typedef struct {
 int published;char publication_id[128],url[1024],error[160];
} publish_response_t;
typedef struct {char name[32],endpoint[512],token[512];} destination_t;
static destination_t destinations[2];
static const stnlabz_module_host_t *host_api;
static void erase(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static int bounded(const char *s,size_t max){return s&&memchr(s,0,max)!=NULL;}
static int secure_config(const char *path){
 struct stat st;
 return path&&lstat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
  (st.st_uid==0||st.st_uid==geteuid())&&!(st.st_mode&077);
}
static int copy(char *dst,size_t cap,const char *src){
 size_t n;if(!dst||!src||(n=strlen(src))>=cap)return 0;
 memcpy(dst,src,n+1);return 1;
}
static int safe_token(const char *v){return v&&*v&&!strpbrk(v,"\r\n");}
static int endpoint_allowed(const char *site,const char *url){
 const char *host;
 if(!site||!url||strncmp(url,"https://",8))return 0;
 host=!strcmp(site,"chaos")?"chaos-mvc.org":
      !strcmp(site,"stnlabz")?"stn-labz.com":NULL;
 if(!host)return 0;
 url+=8;
 if(!strncmp(url,"www.",4))url+=4;
 size_t n=strlen(host);
 return !strncmp(url,host,n)&&url[n]=='/'&&url[n+1]&&
   !strchr(url,'#')&&!strchr(url,'@')&&!strchr(url,'?');
}
static int kind_allowed(const char *kind){
 return kind&&(!strcmp(kind,"announcements")||!strcmp(kind,"news")||
  !strcmp(kind,"page")||!strcmp(kind,"posts"));
}
static int input_valid(const publish_request_t *p){
 if(!p||!bounded(p->site,sizeof p->site)||
    !bounded(p->kind,sizeof p->kind)||
    !bounded(p->category,sizeof p->category)||
    !bounded(p->title,sizeof p->title)||
    !bounded(p->markdown,sizeof p->markdown))return 0;
 if(!kind_allowed(p->kind)||!p->title[0]||!p->markdown[0])return 0;
 for(size_t i=0;p->title[i];i++)
  if((unsigned char)p->title[i]<32)return 0;
 return 1;
}
static int load_config(void){
 FILE *fp;char line[1536];int seen[6]={0};
 destination_t local[2]={0};
 if(!secure_config(CONFIG)||(fp=fopen(CONFIG,"r"))==NULL)return 0;
 copy(local[0].name,sizeof local[0].name,"chaos");
 copy(local[1].name,sizeof local[1].name,"stnlabz");
 while(fgets(line,sizeof line,fp)){
  char *key=line,*value,*end;int index=-1,field=-1;
  if(!strchr(line,'\n')&&!feof(fp))goto fail;
  while(isspace((unsigned char)*key))key++;
  if(!*key||*key=='#')continue;
  value=strchr(key,'=');if(!value)goto fail;
  *value++=0;end=key+strlen(key);
  while(end>key&&isspace((unsigned char)end[-1]))*--end=0;
  end=value+strlen(value);
  while(end>value&&isspace((unsigned char)end[-1]))*--end=0;
  if(!strncmp(key,"chaos_",6)){index=0;key+=6;}
  else if(!strncmp(key,"stnlabz_",8)){index=1;key+=8;}
  else goto fail;
  if(!strcmp(key,"endpoint"))field=0;
  else if(!strcmp(key,"token"))field=1;
  else if(!strcmp(key,"enabled"))field=2;
  else goto fail;
  if(field==0){
   if(!endpoint_allowed(local[index].name,value)||!copy(local[index].endpoint,sizeof local[index].endpoint,value))goto fail;
  }else if(field==1){
   if(!safe_token(value)||!copy(local[index].token,sizeof local[index].token,value))goto fail;
  }else if(strcmp(value,"true")&&strcmp(value,"false"))goto fail;
  seen[index*3+field]=field==2?(!strcmp(value,"true")?1:2):1;
 }
 if(ferror(fp))goto fail;
 fclose(fp);
 for(int i=0;i<2;i++){
  if(seen[i*3+2]==1){
   if(seen[i*3]!=1||seen[i*3+1]!=1)goto fail_closed;
  }else {erase(local[i].token,sizeof local[i].token);local[i].endpoint[0]=0;}
 }
 memcpy(destinations,local,sizeof local);
 erase(local,sizeof local);erase(line,sizeof line);return 1;
fail:
 fclose(fp);
fail_closed:
 erase(local,sizeof local);erase(line,sizeof line);return 0;
}
typedef struct {char bytes[8192];size_t length;} buffer_t;
static size_t read_response(char *ptr,size_t size,size_t count,void *ctx){
 buffer_t *b=ctx;size_t n;
 if(size&&count>SIZE_MAX/size)return 0;
 n=size*count;
 if(n>=sizeof b->bytes-b->length)return 0;
 memcpy(b->bytes+b->length,ptr,n);b->length+=n;b->bytes[b->length]=0;return n;
}
static const char *json_string(struct json_object *o,const char *key){
 struct json_object *v=NULL;
 if(!o||!json_object_object_get_ex(o,key,&v)||
   !json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
static stnlabz_module_result_t publish(const void *req,size_t bytes,
 void *res,size_t cap,size_t *used,void *ctx){
 const publish_request_t *p=req;
 publish_response_t answer={0};destination_t *dest=NULL;
 CURL *curl=NULL;struct curl_slist *headers=NULL;
 struct json_object *payload=NULL,*parsed=NULL;
 buffer_t received={0};char authorization[560]={0};
 long code=0;int ok=0;
 (void)ctx;
 if(used)*used=0;
 if(!req||bytes!=sizeof *p||!res||cap<sizeof answer||!used||!input_valid(p))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(int i=0;i<2;i++)if(!strcmp(p->site,destinations[i].name))dest=&destinations[i];
 if(!dest||!dest->endpoint[0]||!dest->token[0])return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
 if(snprintf(authorization,sizeof authorization,"Authorization: Bearer %s",dest->token)>=(int)sizeof authorization)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 payload=json_object_new_object();if(!payload)goto done;
 json_object_object_add(payload,"kind",json_object_new_string(p->kind));
 json_object_object_add(payload,"category",json_object_new_string(p->category));
 json_object_object_add(payload,"title",json_object_new_string(p->title));
 json_object_object_add(payload,"content",json_object_new_string(p->markdown));
 json_object_object_add(payload,"format",json_object_new_string("markdown"));
 curl=curl_easy_init();if(!curl)goto done;
 headers=curl_slist_append(headers,authorization);
 headers=curl_slist_append(headers,"Content-Type: application/json");
 curl_easy_setopt(curl,CURLOPT_URL,dest->endpoint);
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,json_object_to_json_string(payload));
 curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(json_object_to_json_string(payload)));
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,read_response);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&received);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 if(curl_easy_perform(curl)!=CURLE_OK)goto done;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
 if(code!=200&&code!=201)goto done;
 parsed=json_tokener_parse(received.bytes);
 if(parsed){
  const char *id=json_string(parsed,"id"),*url=json_string(parsed,"url");
  struct json_object *flag=NULL;
  ok=json_object_object_get_ex(parsed,"published",&flag)&&
      json_object_is_type(flag,json_type_boolean)&&json_object_get_boolean(flag);
  if(ok&&id&&*id&&url&&!strncmp(url,"https://",8)&&
     copy(answer.publication_id,sizeof answer.publication_id,id)&&
     copy(answer.url,sizeof answer.url,url))answer.published=1;
 }
done:
 if(!answer.published)copy(answer.error,sizeof answer.error,
  "Publication not confirmed by a valid server receipt.");
 if(parsed)json_object_put(parsed);
 if(payload)json_object_put(payload);
 curl_slist_free_all(headers);if(curl)curl_easy_cleanup(curl);
 erase(authorization,sizeof authorization);
 memcpy(res,&answer,sizeof answer);*used=sizeof answer;
 return answer.published?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static void check(stnlabz_module_qualification_result_t *r,int pass,int negative){
 r->tests_executed++;if(pass)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(pass)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 publish_request_t p={0};char out[8]={0};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 check(r,endpoint_allowed("chaos","https://chaos-mvc.org/api/publish"),0);
 check(r,endpoint_allowed("stnlabz","https://stn-labz.com/api/publish"),0);
 check(r,endpoint_allowed("chaos","https://www.chaos-mvc.org/api/publish"),0);
 check(r,!endpoint_allowed("chaos","http://chaos-mvc.org/api/publish"),1);
 check(r,!endpoint_allowed("chaos","https://chaos-mvc.org.evil.test/"),1);
 check(r,!endpoint_allowed("chaos","https://evil.test/api"),1);
 check(r,!endpoint_allowed("chaos","https://chaos-mvc.org/"),1);
 check(r,!endpoint_allowed("chaos","https://chaos-mvc.org@evil.test/"),1);
 check(r,!endpoint_allowed("other","https://chaos-mvc.org/api"),1);
 check(r,kind_allowed("news"),0);
 check(r,kind_allowed("announcements"),0);
 check(r,kind_allowed("page"),0);
 check(r,kind_allowed("posts"),0);
 check(r,!kind_allowed("admin"),1);
 check(r,!kind_allowed(NULL),1);
 check(r,copy(out,sizeof out,"hello"),0);
 check(r,!copy(out,sizeof out,"12345678"),1);
 check(r,!copy(NULL,sizeof out,"a"),1);
 check(r,!copy(out,sizeof out,NULL),1);
 check(r,safe_token("credential"),0);
 check(r,!safe_token("ab\ncd"),1);
 check(r,!safe_token(""),1);
 check(r,!secure_config("/definitely/missing/publisher.conf"),1);
 copy(p.site,sizeof p.site,"chaos");
 copy(p.kind,sizeof p.kind,"news");
 copy(p.title,sizeof p.title,"Test");
 copy(p.markdown,sizeof p.markdown,"# Test");
 check(r,input_valid(&p),0);
 p.markdown[0]=0;check(r,!input_valid(&p),1);
 copy(p.markdown,sizeof p.markdown,"# Test");
 copy(p.kind,sizeof p.kind,"admin");check(r,!input_valid(&p),1);
 return r->tests_failed||r->tests_executed<STNLABZ_MODULE_MIN_TESTS||
  !r->negative_test_executed||!r->negative_test_passed?
  STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 if(host_api||!host||!host->register_service||!host->unregister_service||
   !host->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!load_config())return STNLABZ_MODULE_ERR_START_FAILED;
 if(!host->register_service(SERVICE,publish,NULL)){
  erase(destinations,sizeof destinations);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host_api=host;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host_api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host_api->unregister_service(SERVICE,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host_api=NULL;erase(destinations,sizeof destinations);return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="publisher",.name="Digit Markdown Publisher",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
