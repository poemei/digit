#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "research.h"

#define SEARCH_ENDPOINT "https://api.search.brave.com/res/v1/web/search"
#define KEY_FILE "/opt/digit/auth/research/brave.key"
#define MAX_HTTP 65536U
typedef struct {char body[MAX_HTTP+1];size_t length;} received_t;
static const stnlabz_module_host_t *host;
static int command_registered,service_registered;
static pthread_mutex_t rate_mutex=PTHREAD_MUTEX_INITIALIZER;
static time_t rate_window;
static unsigned rate_used;
static size_t receive(char *ptr,size_t size,size_t count,void *context)
{
 received_t *r=context;size_t n;
 if(!size||count>SIZE_MAX/size)return 0;
 n=size*count;
 if(n>MAX_HTTP-r->length)return 0;
 memcpy(r->body+r->length,ptr,n);
 r->length+=n;r->body[r->length]=0;
 return n;
}
static int append(char *out,size_t cap,size_t *used,const char *fmt,...)
{
 va_list ap;int n;
 if(*used>=cap)return 0;
 va_start(ap,fmt);n=vsnprintf(out+*used,cap-*used,fmt,ap);va_end(ap);
 if(n<0||(size_t)n>=cap-*used)return 0;
 *used+=(size_t)n;return 1;
}
static void plain(char *out,size_t cap,const char *input)
{
 size_t n=0;int space=0;
 if(!cap)return;
 if(!input){out[0]=0;return;}
 for(;*input&&n+1<cap;++input){
  unsigned char c=(unsigned char)*input;
  if(c=='<'||c=='>'){space=1;continue;}
  if(c<32||c==127||c=='#'||c=='{'||c=='}'){space=1;continue;}
  if(isspace(c)){space=1;continue;}
  if(space&&n&&n+1<cap)out[n++]=' ';
  space=0;
  out[n++]=(char)c;
 }
 out[n]=0;
}
/* Search excerpts are evidence leads, never trusted instructions. */
static const char *jtext(struct json_object *obj,const char *key)
{
 struct json_object *v=NULL;
 return obj&&json_object_object_get_ex(obj,key,&v)&&
  json_object_is_type(v,json_type_string)?json_object_get_string(v):NULL;
}
static int forbidden(const char *query)
{
 char q[RESEARCH_QUERY_MAX];size_t i,n=strlen(query);
 const char *unsafe[]={
  "how to build a bomb","how to make a bomb","make an explosive",
  "how to steal passwords","steal credentials","buy stolen passwords",
  "how to deploy ransomware","write ransomware","create ransomware",
  "how to exploit a child","child sexual abuse material",
  "how to make poison to kill","how to bypass law enforcement",NULL
 };
 if(n<2||n>=sizeof q)return 1;
 for(i=0;i<n;++i)q[i]=(char)tolower((unsigned char)query[i]);q[n]=0;
 for(i=0;unsafe[i];++i)if(strstr(q,unsafe[i]))return 1;
 for(i=0;i<n;++i){
  if((unsigned char)query[i]<32||(unsigned char)query[i]==127)return 1;
 }
 return 0;
}
static int read_key(char *key,size_t cap)
{
 struct stat st;FILE *f;size_t n,i;
 if(stat(KEY_FILE,&st)||!S_ISREG(st.st_mode)||(st.st_mode&077)||
    (st.st_uid!=geteuid()&&st.st_uid!=0))return 0;
 f=fopen(KEY_FILE,"r");if(!f)return 0;
 if(!fgets(key,(int)cap,f)){fclose(f);return 0;}
 if(!feof(f)){char tail[2];if(fgets(tail,sizeof tail,f)){fclose(f);return 0;}}
 fclose(f);
 n=strcspn(key,"\r\n");key[n]=0;
 if(n<12||n>200)return 0;
 for(i=0;i<n;++i)if(!isalnum((unsigned char)key[i])&&
  key[i]!='-'&&key[i]!='_')return 0;
 return 1;
}
static int permitted_rate(void)
{
 time_t now=time(NULL);int allowed=0;
 pthread_mutex_lock(&rate_mutex);
 if(rate_window==0||now<rate_window||now-rate_window>=60){
  rate_window=now;rate_used=0;
 }
 if(rate_used<12){++rate_used;allowed=1;}
 pthread_mutex_unlock(&rate_mutex);
 return allowed;
}
static void audit(const char *query,const char *status,unsigned sources)
{
 uint64_t h=UINT64_C(14695981039346656037);
 const unsigned char *p=(const unsigned char *)query;
 while(*p){h^=*p++;h*=UINT64_C(1099511628211);}
 fprintf(stderr,"[RESEARCH] query_hash=%016llx disposition=%s sources=%u\n",
  (unsigned long long)h,status,sources);
}
/* Provider material is untrusted data. Distinct HTTPS URLs are cited; neither
 * model instructions nor external HTTP actions are accepted from results. */
static unsigned extract_results(const char *body,size_t len,research_response_t *response)
{
 struct json_tokener *tok=json_tokener_new();
 struct json_object *root=NULL,*web=NULL,*rows=NULL;
 size_t i,used=0;unsigned found=0;
 if(!append(response->evidence,sizeof response->evidence,&used,
    "Unverified search excerpts; check original sources before making factual claims.\\n\\n"))return 0;
 if(!tok)return 0;
 root=json_tokener_parse_ex(tok,body,(int)len);
 if(json_tokener_get_error(tok)!=json_tokener_success||!root||
    !json_object_object_get_ex(root,"web",&web)||
    !json_object_object_get_ex(web,"results",&rows)||
    !json_object_is_type(rows,json_type_array))goto done;
 for(i=0;i<json_object_array_length(rows)&&found<5;++i){
  struct json_object *record=json_object_array_get_idx(rows,i);
  const char *url=jtext(record,"url"),*title=jtext(record,"title");
  const char *description=jtext(record,"description");
  char clean_title[180],clean_desc[320],clean_url[460];size_t j;
  if(!url||strncmp(url,"https://",8)||!title)continue;
  if(strlen(url)>440||strchr(url,'@')||strchr(url,'#'))continue;
  for(j=8;url[j]&&url[j]!='/';++j)
   if(!isalnum((unsigned char)url[j])&&url[j]!='-'&&url[j]!='.')break;
  if(url[j]&&url[j]!='/')continue;
  if(j<=8)continue;
  if(!strncmp(url+8,"localhost",9)||!strncmp(url+8,"127.",4)||
     !strncmp(url+8,"10.",3)||!strncmp(url+8,"192.168.",8))continue;
  plain(clean_url,sizeof clean_url,url);
  plain(clean_title,sizeof clean_title,title);
  plain(clean_desc,sizeof clean_desc,description);
  if(!clean_title[0]||!clean_url[0])continue;
  if(!append(response->evidence,sizeof response->evidence,&used,
     "[%u] %s\nSource: %s\nExcerpt: %s\n\n",
     found+1,clean_title,clean_url,clean_desc))break;
  ++found;
 }
done:
 if(root)json_object_put(root);
 json_tokener_free(tok);
 response->sources=found;
 return found;
}
static int search(const char *query,research_response_t *output)
{
 CURL *curl=NULL;struct curl_slist *headers=NULL;
 received_t *rec=NULL;char key[256]={0},auth[320],url[1400];
 char *encoded=NULL;CURLcode rc;long status=0;time_t now=time(NULL);
 struct tm utc;int ok=0;
 memset(output,0,sizeof *output);
 if(forbidden(query)){
  output->status=RESEARCH_DENIED;
  snprintf(output->evidence,sizeof output->evidence,
   "Request denied by the research safety gate.");
  audit(query,"denied",0);return 0;
 }
 if(!permitted_rate()){
  output->status=RESEARCH_UNAVAILABLE;
  snprintf(output->evidence,sizeof output->evidence,
   "Live research rate limit reached; try another request later.");
  audit(query,"rate-limited",0);return 0;
 }
 if(!read_key(key,sizeof key)){
  output->status=RESEARCH_UNAVAILABLE;
  snprintf(output->evidence,sizeof output->evidence,
   "Live research unavailable: approved search credentials are not configured.");
  audit(query,"unavailable",0);return 0;
 }
 curl=curl_easy_init();rec=calloc(1,sizeof *rec);
 if(!curl||!rec)goto done;
 encoded=curl_easy_escape(curl,query,0);if(!encoded)goto done;
 if(snprintf(url,sizeof url,SEARCH_ENDPOINT "?count=5&safesearch=strict&q=%s",encoded)>=(int)sizeof url)goto done;
 snprintf(auth,sizeof auth,"X-Subscription-Token: %s",key);
 headers=curl_slist_append(headers,"Accept: application/json");
 headers=curl_slist_append(headers,auth);
 if(!headers)goto done;
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,4L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,10L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,rec);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-Research/1.0");
 rc=curl_easy_perform(curl);
 if(rc!=CURLE_OK)goto done;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 if(status!=200L||!rec->length)goto done;
 if(!extract_results(rec->body,rec->length,output))goto done;
 output->status=RESEARCH_EVIDENCE;
 if(now>0&&gmtime_r(&now,&utc))
  strftime(output->retrieved_utc,sizeof output->retrieved_utc,
           "%Y-%m-%d %H:%M:%S UTC",&utc);
 ok=1;
done:
 memset(key,0,sizeof key);memset(auth,0,sizeof auth);
 if(encoded)curl_free(encoded);
 if(headers)curl_slist_free_all(headers);
 if(curl)curl_easy_cleanup(curl);
 free(rec);
 if(!ok){
  output->status=RESEARCH_UNAVAILABLE;
  snprintf(output->evidence,sizeof output->evidence,
   "No verified live search evidence is available. Do not invent an answer.");
 }
 audit(query,ok?"evidence":"unavailable",output->sources);
 return ok;
}
static int selftest(unsigned *executed,unsigned *passed,
                    int *negative_executed,int *negative_passed)
{
 research_response_t r={0};
 const char *fixture="{\"web\":{\"results\":["
 "{\"title\":\"Launch schedule\",\"url\":\"https://example.org/launch\","
 "\"description\":\"Updated schedule\"},"
 "{\"title\":\"Unsafe local link\",\"url\":\"http://127.0.0.1/x\","
 "\"description\":\"Not public\"}]}}";
 unsigned e=0,p=0;int ne=0,np=0;
 #define CHECK(x) do{++e;if(x)++p;}while(0)
 CHECK(!forbidden("next SpaceX launch"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(forbidden(""));
 CHECK(extract_results(fixture,strlen(fixture),&r)==1);
 CHECK(r.sources==1);
 CHECK(strstr(r.evidence,"https://example.org/launch")!=NULL);
 CHECK(strstr(r.evidence,"127.0.0.1")==NULL);
 CHECK(extract_results("{}",2,&r)==0);
 ne=1;np=extract_results("{",1,&r)==0;
 #undef CHECK
 if(executed)*executed=e;
 if(passed)*passed=p;
 if(negative_executed)*negative_executed=ne;
 if(negative_passed)*negative_passed=np;
 return e==10&&p==10&&ne==1&&np==1;
}
#ifndef RESEARCH_TEST
static stnlabz_module_result_t service(const void *req,size_t len,void *res,
 size_t cap,size_t *used,void *ctx)
{
 const research_request_t *request=req;
 research_response_t *reply=res;(void)ctx;
 if(!request||len!=sizeof *request||!reply||
    cap<sizeof *reply||!used||
    !memchr(request->query,0,sizeof request->query))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *used=sizeof *reply;
 search(request->query,reply);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 research_response_t result;char message[900];
 (void)unused;
 if(!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!cmd->arguments[0])
  return reply(context,"Usage: /research <question>")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 search(cmd->arguments,&result);
 if(result.status!=RESEARCH_EVIDENCE)
  snprintf(message,sizeof message,"%s",result.evidence);
 else
  snprintf(message,sizeof message,
   "Live research %s; %u attributed sources. %.620s",
   result.retrieved_utc,result.sources,result.evidence);
 return reply(context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 {unsigned e=0,p=0;int ne=0,np=0;
  int valid=selftest(&e,&p,&ne,&np);
  q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
  q->negative_test_executed=ne;q->negative_test_passed=np;
  return valid?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
 }
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
 if(!h||!h->register_command||!h->unregister_command||
    !h->register_service||!h->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)
  return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service("digit.research.search.v1",service,NULL)){
  curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 service_registered=1;
 if(!h->register_command("research",command,NULL)){
  h->unregister_service("digit.research.search.v1",NULL);
  service_registered=0;curl_global_cleanup();
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 command_registered=1;host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(host){
  if(command_registered)host->unregister_command("research",NULL);
  if(service_registered)host->unregister_service("digit.research.search.v1",NULL);
 }
 host=NULL;command_registered=service_registered=0;
 curl_global_cleanup();return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "research","Digit Live Research",1,0,0,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
#endif
