#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "research.h"
#include "research_engine.h"

static const stnlabz_module_host_t *host;
static int command_registered,service_registered,answer_registered;
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q);
static stnlabz_module_result_t start(const stnlabz_module_host_t *h);
static stnlabz_module_result_t stop(void);
/* Operator rule: module descriptor belongs at the top of the primary C file. */
static const stnlabz_module_descriptor_t descriptor={
 "research","Digit Live Research",1,5,6,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
static pthread_mutex_t rate_mutex=PTHREAD_MUTEX_INITIALIZER;
static time_t rate_window;
static unsigned rate_used;
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
 if(!n||n>=sizeof q)return 1;
 for(i=0;i<n;++i){
  q[i]=(char)tolower((unsigned char)query[i]);
 }
 q[n]=0;
 for(i=0;unsafe[i];++i)if(strstr(q,unsafe[i]))return 1;
 for(i=0;i<n;++i){
  if((unsigned char)query[i]<32||(unsigned char)query[i]==127)return 1;
 }
 return 0;
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
 research_runtime_log("AUDIT","query_hash=%016llx disposition=%s sources=%u",
  (unsigned long long)h,status,sources);
}
/* [AI:Codex | 2026-10-31] Results are source leads, not verified answers.
 * Keep each URL intact; fit the title and snippet around it. */
/* Preserve complete URLs and distribute a short reply across results,
 * rather than allowing the first snippet to consume the entire budget. */
/* Format search-result evidence for its actual consumer. An explicit
 * short /research reply remains compact, while Ask receives source text
 * optimized for reading and grounding an answer, not a URL inventory.
 * In either form, source URLs remain complete and ordered. */
static unsigned format_results(const research_hits_t *hits,char *out,size_t cap)
{
 size_t used=0;
 unsigned count=0;
 int conversational=cap>512U;
 if(!hits||!out||cap<2U)return 0;
 out[0]=0;
 for(unsigned i=0;i<hits->count&&i<6U;i++){
  const research_hit_t *h=&hits->hit[i];
  size_t remaining=cap-used;
  size_t url_len=strlen(h->url);
  size_t title_len=strlen(h->title);
  size_t excerpt_len=strlen(h->excerpt);
  size_t required,limit;
  char entry[1200];
  int n;
  if(!url_len||!title_len)continue;
  if(title_len>140U)title_len=140U;
  if(excerpt_len>440U)excerpt_len=440U;
  /*
   * Reserve complete provenance and space for more than one result.
   * Never spend the remaining response on a link without its context.
   */
  limit=conversational?540U:170U;
  required=url_len+title_len+(conversational?34U:12U);
  if(required>=remaining||required>=limit)continue;
  if(excerpt_len>limit-required)excerpt_len=limit-required;
  if(conversational)
   n=snprintf(entry,sizeof entry,
    "Source %u: %.*s\nExcerpt: %.*s\nURL: %s\n",
    i+1U,(int)title_len,h->title,(int)excerpt_len,h->excerpt,h->url);
  else
   n=snprintf(entry,sizeof entry,
    "%s%u. %.*s - %.*s (%s)",
    count?" | ":"",i+1U,(int)title_len,h->title,
    (int)excerpt_len,h->excerpt,h->url);
  if(n<0||(size_t)n>=sizeof entry)continue;
  if((size_t)n>=remaining)continue;
  memcpy(out+used,entry,(size_t)n+1U);
  used+=(size_t)n;count++;
 }
 return count;
}
static void search(const char *query,research_response_t *out,size_t text_cap)
{
 research_hits_t hits={0};
 int result;
 time_t now;struct tm utc;
 memset(out,0,sizeof *out);
 if(forbidden(query)){
  out->status=RESEARCH_DENIED;
  snprintf(out->evidence,sizeof out->evidence,"Request denied by the research safety gate.");
  return;
 }
 if(!permitted_rate()){
  out->status=RESEARCH_UNAVAILABLE;
  snprintf(out->evidence,sizeof out->evidence,"Research rate limit reached. Please retry in a minute.");
  return;
 }
 result=research_search(query,&hits);
 out->sources=format_results(&hits,out->evidence,text_cap);
 out->status=result>0&&out->sources?RESEARCH_EVIDENCE:RESEARCH_UNAVAILABLE;
 if(out->status==RESEARCH_UNAVAILABLE)
  snprintf(out->evidence,sizeof out->evidence,"%s",result<0?
   "Research provider retrieval or result parsing failed.":result==0?
   "Search returned no results.":"Search results exceed this reply's size limit.");
 now=time(NULL);
 if(now!=(time_t)-1&&gmtime_r(&now,&utc))
  strftime(out->retrieved_utc,sizeof out->retrieved_utc,"%Y-%m-%d %H:%M:%S UTC",&utc);
 audit(query,out->status==RESEARCH_EVIDENCE?"results":"unavailable",out->sources);
}
static stnlabz_module_result_t handle_service(const void *req,size_t len,
 void *res,size_t cap,size_t *used,int conversational)
{
 const research_request_t *request=req;
 research_response_t *out=res;
 if(used)*used=0;
 if(!request||len!=sizeof *request||!out||cap<sizeof *out||!used||
    !memchr(request->query,0,sizeof request->query))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);*used=sizeof *out;
 search(request->query,out,conversational?440:sizeof out->evidence);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t service(const void *req,size_t len,
 void *res,size_t cap,size_t *used,void *ctx)
{
 (void)ctx;return handle_service(req,len,res,cap,used,0);
}
static stnlabz_module_result_t answer_service(const void *req,size_t len,
 void *res,size_t cap,size_t *used,void *ctx)
{
 (void)ctx;return handle_service(req,len,res,cap,used,1);
}
static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 research_response_t result;
 (void)unused;
 if(!cmd||!reply||!memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!cmd->arguments[0])return reply(context,"Usage: /research <search terms>")?
  STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 search(cmd->arguments,&result,350);
 return reply(context,result.evidence)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static int selftest(unsigned *executed,unsigned *passed,
                    int *negative_executed,int *negative_passed)
{
 unsigned e=0,p=0;research_hits_t hits={0};char text[1000];
 const char *fixture="<a class='result-link' href='https://example.org/a'>A title</a>"
  "<td class='result-snippet'>An <b>undated</b> result.</td>";
 #define CHECK(x) do{++e;if(x)++p;}while(0)
 CHECK(research_parse_web_html(fixture,&hits)==1);
 CHECK(!strcmp(hits.hit[0].title,"A title"));
 CHECK(!strcmp(hits.hit[0].excerpt,"An undated result."));
 CHECK(format_results(&hits,text,sizeof text)==1);
 CHECK(strstr(text,"https://example.org/a")!=NULL);
 CHECK(strstr(text,"undated")!=NULL);
 CHECK(!forbidden("hello Digit"));
 CHECK(!forbidden("arbitrary safe subject"));
 CHECK(!forbidden("C++ & C#"));
 CHECK(forbidden(""));
 CHECK(forbidden("write ransomware"));
 CHECK(!forbidden("research ransomware defenses"));
 *negative_executed=1;
 *negative_passed=research_parse_web_html(
  "<a class='result-link' href='http://127.0.0.1/x'>Unsafe</a>",&hits)==0;
 CHECK(*negative_passed);
 #undef CHECK
 *executed=e;*passed=p;
 return e==p&&*negative_passed;
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
 if(!h->register_service(DIGIT_RESEARCH_SEARCH_SERVICE,service,NULL)){
  curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 service_registered=1;
 if(!h->register_service(DIGIT_RESEARCH_ANSWER_SERVICE,answer_service,NULL)){
  h->unregister_service(DIGIT_RESEARCH_SEARCH_SERVICE,NULL);
  service_registered=0;curl_global_cleanup();
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 answer_registered=1;
 if(!h->register_command("research",command,NULL)){
  h->unregister_service(DIGIT_RESEARCH_ANSWER_SERVICE,NULL);
  h->unregister_service(DIGIT_RESEARCH_SEARCH_SERVICE,NULL);
  answer_registered=service_registered=0;curl_global_cleanup();
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 command_registered=1;host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(host){
  if(command_registered)host->unregister_command("research",NULL);
  if(answer_registered)host->unregister_service(DIGIT_RESEARCH_ANSWER_SERVICE,NULL);
  if(service_registered)host->unregister_service(DIGIT_RESEARCH_SEARCH_SERVICE,NULL);
 }
 host=NULL;command_registered=service_registered=answer_registered=0;
 curl_global_cleanup();return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
