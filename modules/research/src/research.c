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

#define SEARCH_ENDPOINT "https://www.bing.com/search"
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
/* Bounded RSS parser for a fixed, keyless, HTTPS search endpoint.
 * Search snippets are UNVERIFIED leads. No arbitrary URL is fetched. */
static int unescape_xml(char *dest,size_t cap,const char *first,size_t len)
{
 size_t i=0,j=0;int in_tag=0;
 while(i<len&&j+1<cap){
  if(first[i]=='<'){in_tag=1;i++;continue;}
  if(first[i]=='>'){in_tag=0;i++;continue;}
  if(in_tag){i++;continue;}
  if(first[i]=='&'){
   static const struct {const char *entity;char replacement;} entities[]={
    {"&amp;",'&'},{"&quot;",'"'},{"&apos;",39},
    {"&lt;",'<'},{"&gt;",'>'},{"&#39;",39}
   };
   size_t k;int found=0;
   for(k=0;k<sizeof entities/sizeof entities[0];k++){
    size_t n=strlen(entities[k].entity);
    if(n<=len-i&&!memcmp(first+i,entities[k].entity,n)){
     dest[j++]=entities[k].replacement;i+=n;found=1;break;
    }
   }
   if(!found){i++;dest[j++]=' ';}continue;
  }
  if((unsigned char)first[i]>=32&&first[i]!='{'&&first[i]!='}'&&
      first[i]!='#')dest[j++]=first[i];
  else dest[j++]=' ';
  i++;
 }
 dest[j]=0;return j>0;
}
static int item_value(const char *first,const char *last,
                      const char *tag,char *dest,size_t cap)
{
 char open[64],close[64];const char *p,*end;
 if(snprintf(open,sizeof open,"<%s>",tag)>=(int)sizeof open||
    snprintf(close,sizeof close,"</%s>",tag)>=(int)sizeof close)return 0;
 p=strstr(first,open);
 if(!p||p>=last)return 0;
 p+=strlen(open);
 end=strstr(p,close);
 if(!end||end>last)return 0;
 return unescape_xml(dest,cap,p,(size_t)(end-p));
}
static int safe_source_url(const char *url)
{
 const char *p;size_t host_len;
 if(strncmp(url,"https://",8))return 0;
 p=url+8;
 while(*p&&*p!='/'&&*p!='?'&&*p!='#')p++;
 host_len=(size_t)(p-(url+8));
 if(host_len<4||host_len>128||!memchr(url+8,'.',host_len))return 0;
 if(strchr(url,'@')||strchr(url,'\\')||strchr(url,'#'))return 0;
 if(!strncmp(url+8,"localhost",9)||!strncmp(url+8,"127.",4)||
    !strncmp(url+8,"10.",3)||!strncmp(url+8,"192.168.",8)||
    !strncmp(url+8,"169.254.",8)||!strncmp(url+8,"172.16.",7))return 0;
 for(size_t i=8;i<8+host_len;i++){
  unsigned char c=(unsigned char)url[i];
  if(!isalnum(c)&&c!='.'&&c!='-')return 0;
 }
 return 1;
}
/* Keep subject terms, not conversational filler such as "when is the next".
 * The same terms are required in result metadata before any lead is accepted. */
typedef struct {char term[6][40];unsigned count;char search[256];} topic_t;
static int stopword(const char *word)
{
 static const char *words[]={
  "a","an","and","are","at","be","can","could","do","does","for","from",
  "how","i","in","is","it","latest","me","my","next","of","on","or",
  "please","rocket","show","tell","the","this","to","today","tomorrow",
  "upcoming","us","what","when","where","which","who","will","with",
  "you","your",NULL
 };
 unsigned j;
 for(j=0;words[j];++j)if(!strcmp(word,words[j]))return 1;
 return 0;
}
static int make_topic(const char *question,topic_t *topic)
{
 const unsigned char *p=(const unsigned char *)question;size_t used=0;
 memset(topic,0,sizeof *topic);
 while(*p&&topic->count<6){
  char word[40];size_t n=0;unsigned i;
  while(*p&&!isalnum(*p))p++;
  while(*p&&isalnum(*p)){
   if(n+1<sizeof word)word[n++]=(char)tolower(*p);
   p++;
  }
  if(!n)break;
  word[n]=0;
  if(n<3||stopword(word))continue;
  for(i=0;i<topic->count;i++)if(!strcmp(word,topic->term[i]))break;
  if(i<topic->count)continue;
  memcpy(topic->term[topic->count++],word,n+1);
  if(used+n+2>=sizeof topic->search)break;
  if(used)topic->search[used++]=' ';
  memcpy(topic->search+used,word,n);used+=n;topic->search[used]=0;
 }
 return topic->count>0;
}
static int contains_token(const char *haystack,const char *needle)
{
 size_t len=strlen(needle);const unsigned char *p=(const unsigned char *)haystack;
 if(!len)return 0;
 while(*p){
  size_t i=0;
  if(p!=(const unsigned char *)haystack&&isalnum(p[-1])){p++;continue;}
  while(i<len&&p[i]&&tolower(p[i])==(unsigned char)needle[i])i++;
  if(i==len&&!isalnum(p[i]))return 1;
  p++;
 }
 return 0;
}
static int relevant(const topic_t *topic,const char *title,
                    const char *description,const char *link)
{
 unsigned matches=0,i;
 char combined[1100];
 snprintf(combined,sizeof combined,"%s %s %s",title,description,link);
 /* Require the primary subject (e.g. SpaceX) AND a second subject when
  * present; a retailer called "Next" cannot satisfy either requirement. */
 if(!topic->count||!contains_token(combined,topic->term[0]))return 0;
 for(i=1;i<topic->count;i++)
  if(contains_token(combined,topic->term[i]))matches++;
 return topic->count==1||matches>0;
}
static unsigned extract_results(const char *body,size_t len,const topic_t *topic,research_response_t *response)
{
 size_t used=0;unsigned found=0;
 const char *cursor,*end;
 if(!body||len<20||len>MAX_HTTP)return 0;
 if(!strstr(body,"<rss")&&!strstr(body,"<feed"))return 0;
 cursor=body;end=body+len;
 while(found<5){
  const char *start=strstr(cursor,"<item>");
  const char *finish;
  char title[180]={0},link[480]={0},summary[320]={0},date[96]={0};
  if(!start||start>=end)break;
  finish=strstr(start,"</item>");
  if(!finish||finish>=end)break;
  cursor=finish+7;
  if(!item_value(start,finish,"title",title,sizeof title)||
     !item_value(start,finish,"link",link,sizeof link)||
     !safe_source_url(link))continue;
  (void)item_value(start,finish,"description",summary,sizeof summary);
  (void)item_value(start,finish,"pubDate",date,sizeof date);
  if(!relevant(topic,title,summary,link))continue;
  if(!append(response->evidence,sizeof response->evidence,&used,
    "%s — %s (feed: %s). %s\n",
    title,link,date[0]?date:"unknown",
    summary[0]?summary:""))break;
  found++;
 }
 response->sources=found;
 return found;
}
static int search(const char *query,research_response_t *output)
{
 CURL *curl=NULL;received_t *rec=NULL;
 char url[1400];char *encoded=NULL;topic_t topic;
 CURLcode rc;long status=0;time_t now=time(NULL);
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
    "Live research rate limit reached.");
  audit(query,"rate-limited",0);return 0;
 }
 if(!make_topic(query,&topic))goto done;
 /* Favor upcoming, recently scheduled events without treating "next" as
  * a subject (the clothing-site false positive). */
 if(strstr(query,"next")||strstr(query,"Next")){
  if(strstr(topic.search,"launch")){
   size_t n=strlen(topic.search);
   if(n+18<sizeof topic.search)
    memcpy(topic.search+n," upcoming schedule",18);
  }
 }
 curl=curl_easy_init();rec=calloc(1,sizeof *rec);
 if(!curl||!rec)goto done;
 encoded=curl_easy_escape(curl,topic.search,0);
 if(!encoded)goto done;
 if(snprintf(url,sizeof url,SEARCH_ENDPOINT "?format=rss&q=%s",encoded)
    >=(int)sizeof url)goto done;
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,4L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,10L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,rec);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-Research/1.1");
 rc=curl_easy_perform(curl);
 if(rc!=CURLE_OK)goto done;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 if(status!=200L||!rec->length)goto done;
 if(!extract_results(rec->body,rec->length,&topic,output))goto done;
 output->status=RESEARCH_EVIDENCE;
 if(now>0&&gmtime_r(&now,&utc))
  strftime(output->retrieved_utc,sizeof output->retrieved_utc,
           "%Y-%m-%d %H:%M:%S UTC",&utc);
 ok=1;
done:
 if(encoded)curl_free(encoded);
 if(curl)curl_easy_cleanup(curl);
 free(rec);
 if(!ok){
  output->status=RESEARCH_UNAVAILABLE;
  snprintf(output->evidence,sizeof output->evidence,
   "Live search evidence unavailable. Do not invent an answer.");
 }
 audit(query,ok?"evidence":"unavailable",output->sources);
 return ok;
}
static int selftest(unsigned *executed,unsigned *passed,
                    int *negative_executed,int *negative_passed)
{
 research_response_t r={0};topic_t topic;
 const char *fixture="<?xml version=\"1.0\"?><rss><channel><item>"
 "<title>SpaceX launch schedule</title><link>https://example.org/launch</link>"
 "<pubDate>Fri, 30 Oct 2026 12:00:00 GMT</pubDate>"
 "<description>SpaceX rocket launches and updates</description></item>"
 "<item><title>Next US Official Site Fashion</title><link>https://www.next.us/en</link></item>"
 "<item><title>Bad</title><link>http://127.0.0.1/x</link></item>"
 "</channel></rss>";
 unsigned e=0,p=0;int ne=0,np=0;
 #define CHECK(x) do{++e;if(x)++p;}while(0)
 CHECK(!forbidden("next SpaceX launch"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(forbidden(""));
 CHECK(make_topic("when is the next SpaceX rocket launch?",&topic));
 CHECK(extract_results(fixture,strlen(fixture),&topic,&r)==1);
 CHECK(r.sources==1);
 CHECK(strstr(r.evidence,"https://example.org/launch")!=NULL);
 CHECK(strstr(r.evidence,"127.0.0.1")==NULL);
 CHECK(strstr(r.evidence,"next.us")==NULL);
 CHECK(!strcmp(topic.search,"spacex launch"));
 CHECK(extract_results("{}",2,&topic,&r)==0);
 ne=1;np=extract_results("<rss><item>",11,&topic,&r)==0;
 #undef CHECK
 if(executed)*executed=e;if(passed)*passed=p;
 if(negative_executed)*negative_executed=ne;
 if(negative_passed)*negative_passed=np;
 return e>=10&&e==p&&ne==1&&np==1;
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
 research_response_t result;char message[380];
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
   "Research %s; %u sources: %.235s",
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
 "research","Digit Live Research",1,1,2,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
#endif
