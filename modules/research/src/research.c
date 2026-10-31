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
#include <time.h>
#include <unistd.h>
#include "research.h"
#include "research_engine.h"

#define SEARCH_ENDPOINT "https://www.bing.com/search"
#define MAX_HTTP (512U * 1024U)
typedef struct {char body[MAX_HTTP+1];size_t length;} received_t;
static const stnlabz_module_host_t *host;
#ifndef RESEARCH_TEST
static int command_registered,service_registered,answer_registered;
#endif
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
  char title[180]={0},link[480]={0},summary[320]={0};
  if(!start||start>=end)break;
  finish=strstr(start,"</item>");
  if(!finish||finish>=end)break;
  cursor=finish+7;
  if(!item_value(start,finish,"title",title,sizeof title)||
     !item_value(start,finish,"link",link,sizeof link)||
     !safe_source_url(link))continue;
  (void)item_value(start,finish,"description",summary,sizeof summary);
  if(!relevant(topic,title,summary,link))continue;
  if(!append(response->evidence,sizeof response->evidence,&used,
    "%s. %s\n",
    title,summary[0]?summary:""))break;
  found++;
 }
 response->sources=found;
 return found;
}

/* Structured launch schedules are distinct from news headline searching. */
#define LAUNCH_ENDPOINT "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?lsp__name=SpaceX&limit=15&ordering=net&format=json"
static const char *json_string(struct json_object *object,const char *key)
{
 struct json_object *value=NULL;
 return object&&json_object_object_get_ex(object,key,&value)&&
  json_object_is_type(value,json_type_string)?json_object_get_string(value):NULL;
}
static struct json_object *json_child(struct json_object *object,const char *key)
{
 struct json_object *value=NULL;
 return object&&json_object_object_get_ex(object,key,&value)?value:NULL;
}
static void short_text(char *dest,size_t size,const char *src)
{
 size_t i=0;
 if(!size)return;
 if(!src){dest[0]=0;return;}
 for(;*src&&i+1<size;src++){
  unsigned char c=(unsigned char)*src;
  if(c>=32&&c<127&&c!='<'&&c!='>'&&c!='{'&&c!='}')
   dest[i++]=(char)c;
 }
 dest[i]=0;
}
/* Research alone decides if a request requires current external evidence. */
static int research_claims(const char *question)
{
 char normalized[RESEARCH_QUERY_MAX];
 size_t i,n;int any=0;
 static const char *signals[]={
  "next ","upcoming ","latest ","today","tonight","tomorrow",
  "this week","this month","current ","currently","right now",
  "recent ","live ","schedule","scheduled","launch",
  "news ","weather","forecast","stock price","exchange rate",
  "who won","who is winning","when is","when will",
  "when do","when are","when does","where can","what time",
  "how much is","as of now","opening hours","learn ","study ",
  "research ","investigate ","look up ","find out","find information",
  "teach me","read about","tell me about","search for",
  "what is ","what are ","who is ","how does ","how do ",
  "explain ","describe ",NULL
 };
 if(!question)return 0;
 n=strlen(question);
 if(!n)return 0;
 if(n>=sizeof normalized)return 1;
 for(i=0;i<n;i++){
  unsigned char c=(unsigned char)question[i];
  normalized[i]=(char)tolower(c);
  if(isalpha(c))any=1;
 }
 normalized[n]=0;
 if(!any)return 0;
 for(i=0;signals[i];i++)if(strstr(normalized,signals[i]))return 1;
 return 0;
}
static int launch_question(const char *query)
{
 char q[RESEARCH_QUERY_MAX];size_t i,n=strlen(query);
 if(n>=sizeof q)return 0;
 for(i=0;i<n;i++)q[i]=(char)tolower((unsigned char)query[i]);
 q[n]=0;
 return strstr(q,"spacex")&&
  (strstr(q,"launch")||strstr(q,"liftoff")||strstr(q,"rocket"))&&
  (strstr(q,"when")||strstr(q,"next")||strstr(q,"upcoming")||
   strstr(q,"schedule")||strstr(q,"time"));
}
static int leap(int year){return year%4==0&&(year%100!=0||year%400==0);}
static int month_days(int y,int m)
{
 static const int days[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 return m==2?28+leap(y):days[m-1];
}
/* Gregorian civil date to UTC days since Unix epoch. */
static int64_t epoch_days(int y,unsigned m,unsigned d)
{
 int era,yoe,mp;unsigned doy,doe;
 y-=m<=2;
 era=(y>=0?y:y-399)/400;
 yoe=y-era*400;
 mp=(int)m+(m>2?-3:9);
 doy=(153*(unsigned)mp+2)/5+d-1;
 doe=(unsigned)yoe*365+(unsigned)yoe/4-(unsigned)yoe/100+doy;
 return (int64_t)era*146097+(int64_t)doe-719468;
}
static int read_net(const char *net,int64_t *epoch,int *year,
                     int *month,int *day,int *hour,int *minute)
{
 int second=0;
 if(!net||strlen(net)<19||
    sscanf(net,"%4d-%2d-%2dT%2d:%2d:%2d",
     year,month,day,hour,minute,&second)!=6||
    *year<2020||*year>2100||*month<1||*month>12||
    *day<1||*day>month_days(*year,*month)||
    *hour>23||*hour<0||*minute>59||*minute<0||
    second<0||second>59||net[19]!='Z')return 0;
 *epoch=epoch_days(*year,(unsigned)*month,(unsigned)*day)*86400+
        *hour*3600+*minute*60+second;
 return 1;
}
static int first_sunday(int year,int month)
{
 /* 1970-01-01 was Thursday. Sunday = 0. */
 int day_of_week=(int)((epoch_days(year,(unsigned)month,1)+4)%7);
 if(day_of_week<0)day_of_week+=7;
 return 1+(7-day_of_week)%7;
}
static int pacific_offset(int64_t utc,int year)
{
 int spring=first_sunday(year,3)+7,fall=first_sunday(year,11);
 int64_t starts=epoch_days(year,3,(unsigned)spring)*86400+10*3600;
 int64_t ends=epoch_days(year,11,(unsigned)fall)*86400+9*3600;
 return utc>=starts&&utc<ends?-7:-8;
}
static int format_pacific(int64_t utc,int year,char *out,size_t cap)
{
 static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun",
  "Jul","Aug","Sep","Oct","Nov","Dec"};
 int offset=pacific_offset(utc,year);
 time_t adjusted=(time_t)(utc+(int64_t)offset*3600);
 struct tm local;
 if(!gmtime_r(&adjusted,&local)||local.tm_mon<0||local.tm_mon>11)return 0;
 return snprintf(out,cap,"%s %d, %d at %d:%02d %s",
  months[local.tm_mon],local.tm_mday,local.tm_year+1900,
  local.tm_hour%12?local.tm_hour%12:12,local.tm_min,
  local.tm_hour<12?"AM Pacific":"PM Pacific")<(int)cap;
}
static unsigned launch_answer(const char *data,size_t bytes,
                              time_t now,research_response_t *out)
{
 struct json_tokener *tok=json_tokener_new();
 struct json_object *root=NULL,*rows=NULL,*chosen=NULL;
 int64_t best=INT64_MAX;int chosen_y=0;size_t i;
 char when[90],mission[76],rocket[56],site[76],status[40];
 int found=0;
 if(!tok)return 0;
 root=json_tokener_parse_ex(tok,data,(int)bytes);
 if(json_tokener_get_error(tok)!=json_tokener_success||
    !root||!(rows=json_child(root,"results"))||
    !json_object_is_type(rows,json_type_array))goto done;
 for(i=0;i<json_object_array_length(rows)&&i<30;i++){
  struct json_object *item=json_object_array_get_idx(rows,i);
  struct json_object *provider=json_child(item,"launch_service_provider");
  const char *agency=json_string(provider,"name");
  const char *net=json_string(item,"net");
  struct json_object *st=json_child(item,"status");
  const char *state=json_string(st,"name");
  int y,m,d,h,min;int64_t at;
  if(!agency||strcmp(agency,"SpaceX")||!read_net(net,&at,&y,&m,&d,&h,&min))continue;
  if(state&&(strstr(state,"Success")||strstr(state,"Failure")||
             strstr(state,"In Flight")))continue;
  if(at<(int64_t)now-60||at>=best)continue;
  best=at;chosen=item;chosen_y=y;
 }
 if(!chosen)goto done;
 {
  struct json_object *r=json_child(chosen,"rocket");
  struct json_object *config=json_child(r,"configuration");
  struct json_object *pad=json_child(chosen,"pad");
  struct json_object *location=json_child(pad,"location");
  struct json_object *state=json_child(chosen,"status");
  const char *name=json_string(chosen,"name");
  const char *full=json_string(config,"full_name");
  const char *padname=json_string(location,"name");
  const char *state_name=json_string(state,"name");
  const char *precision=json_string(json_child(chosen,"net_precision"),"name");
  const char *divider=name?strstr(name," | "):NULL;
  if(!name||!format_pacific(best,chosen_y,when,sizeof when))goto done;
  if(precision&&(!strcmp(precision,"Day")||!strcmp(precision,"Month")||
                 !strcmp(precision,"Year")||!strcmp(precision,"Week"))){
   char *at=strstr(when," at ");
   if(at)*at=0;
   if(strlen(when)+16<sizeof when)strcat(when," (time TBD)");
  }
  if(divider)short_text(mission,sizeof mission,divider+3);
  else short_text(mission,sizeof mission,name);
  short_text(rocket,sizeof rocket,full?full:"Rocket unspecified");
  short_text(site,sizeof site,padname?padname:"Launch site unconfirmed");
  short_text(status,sizeof status,state_name?state_name:"Unconfirmed");
  if(!mission[0])goto done;
  if(snprintf(out->evidence,sizeof out->evidence,
     "Next SpaceX launch: %s. Rocket: %s. Launch site: %s. Target: %s. Status: %s.",
     mission,rocket,site,when,status)>=(int)sizeof out->evidence)goto done;
  found=1;
 }
done:
 if(root)json_object_put(root);
 json_tokener_free(tok);
 out->sources=(unsigned)found;
 return (unsigned)found;
}
static int fetch_launch(time_t now,research_response_t *out)
{
 CURL *curl=curl_easy_init();received_t *rec=calloc(1,sizeof *rec);
 CURLcode rc;long status=0;int ok=0;
 if(!curl||!rec)goto done;
 curl_easy_setopt(curl,CURLOPT_URL,LAUNCH_ENDPOINT);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,4L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,10L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,rec);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-Research/1.2");
 rc=curl_easy_perform(curl);
 if(rc!=CURLE_OK)goto done;
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 if(status!=200L||!rec->length)goto done;
 ok=launch_answer(rec->body,rec->length,now,out)>0;
done:
 if(curl)curl_easy_cleanup(curl);
 free(rec);
 return ok;
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
 if(launch_question(query)){
  if(!fetch_launch(now,output))goto done;
  output->status=RESEARCH_EVIDENCE;
  if(now>0&&gmtime_r(&now,&utc))
   strftime(output->retrieved_utc,sizeof output->retrieved_utc,
    "%Y-%m-%d %H:%M:%S UTC",&utc);
  audit(query,"structured-launch",output->sources);
  return 1;
 }
 if(!make_topic(query,&topic))goto done;
 /* Favor upcoming, recently scheduled events without treating "next" as
  * a subject (the clothing-site false positive). */
 if(strstr(query,"next")||strstr(query,"Next")){
  if(strstr(topic.search,"launch")){
   size_t n=strlen(topic.search);
   if(n+19<sizeof topic.search)
    (void)snprintf(topic.search+n,sizeof topic.search-n,"%s"," upcoming schedule");
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
   launch_question(query)?"I could not confirm the next SpaceX launch.":"I could not verify that right now.");
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
 CHECK(research_claims("when is the next SpaceX rocket launch?"));
 CHECK(research_claims("What is the current weather?"));
 CHECK(!research_claims("say hi to rictus"));
 CHECK(!research_claims("hello Digit"));
 CHECK(!forbidden("I"));
 CHECK(!forbidden("hi"));
 CHECK(!forbidden("next SpaceX launch"));
 CHECK(!forbidden("research ransomware defenses"));
 CHECK(forbidden("how to build a bomb"));
 CHECK(forbidden("write ransomware"));
 CHECK(forbidden(""));
 CHECK(make_topic("when is the next SpaceX rocket launch?",&topic));
 CHECK(extract_results(fixture,strlen(fixture),&topic,&r)==1);
 CHECK(r.sources==1);
 CHECK(strstr(r.evidence,"SpaceX launch schedule")!=NULL);
 CHECK(strstr(r.evidence,"https://")==NULL);
 CHECK(strstr(r.evidence,"127.0.0.1")==NULL);
 CHECK(strstr(r.evidence,"next.us")==NULL);
 CHECK(!strcmp(topic.search,"spacex launch"));
 CHECK(extract_results("{}",2,&topic,&r)==0);
 {
 const char *launch_fixture="{\"results\":["
 "{\"name\":\"Falcon 9 | Starlink Group 15-18\","
 "\"net\":\"2026-10-31T04:02:00Z\","
 "\"launch_service_provider\":{\"name\":\"SpaceX\"},"
 "\"rocket\":{\"configuration\":{\"full_name\":\"Falcon 9 Block 5\"}},"
 "\"pad\":{\"location\":{\"name\":\"Vandenberg SFB\"}},"
 "\"status\":{\"name\":\"Go for Launch\"}}]}";
 memset(&r,0,sizeof r);
 CHECK(launch_question("when is the next SpaceX rocket launch?"));
 CHECK(!launch_question("What is TCP?"));
 CHECK(launch_answer(launch_fixture,strlen(launch_fixture),0,&r)==1);
 CHECK(strstr(r.evidence,"Starlink Group 15-18")!=NULL);
 CHECK(strstr(r.evidence,"9:02 PM Pacific")!=NULL);
 CHECK(strstr(r.evidence,"Vandenberg SFB")!=NULL);
 CHECK(strstr(r.evidence,"http")==NULL);
 }
 ne=1;np=extract_results("<rss><item>",11,&topic,&r)==0;
 #undef CHECK
 if(executed)*executed=e;
 if(passed)*passed=p;
 if(negative_executed)*negative_executed=ne;
 if(negative_passed)*negative_passed=np;
 return e>=10&&e==p&&ne==1&&np==1;
}
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
/* Module-owned research intent and answer boundary.
 * An unclaimed message passes to the Ask controller for another model.
 * A claimed but unverified message NEVER goes to offline generation. */
static stnlabz_module_result_t answer_service(const void *req,size_t len,
 void *res,size_t cap,size_t *used,void *ctx)
{
 const research_request_t *request=req;
 research_response_t *reply=res;(void)ctx;
 if(used)*used=0;
 if(!request||len!=sizeof *request||!reply||
    cap<sizeof *reply||!used||
    !memchr(request->query,0,sizeof request->query))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(reply,0,sizeof *reply);
 *used=sizeof *reply;
 if(forbidden(request->query)){
  reply->status=RESEARCH_DENIED;
  snprintf(reply->evidence,sizeof reply->evidence,
   "I can't help with that request.");
  audit(request->query,"denied",0);
  return STNLABZ_MODULE_OK;
 }
 if(!research_claims(request->query)){
  reply->status=RESEARCH_NOT_APPLICABLE;
  return STNLABZ_MODULE_OK;
 }
 if(!permitted_rate()){
  reply->status=RESEARCH_UNAVAILABLE;
  snprintf(reply->evidence,sizeof reply->evidence,
   "I couldn't verify that right now.");
  audit(request->query,"rate-limited",0);
  return STNLABZ_MODULE_OK;
 }
 /* Preserve the already-working structured provider as an optimization,
  * not a limitation: unsupported subjects and failures use general research. */
 if(launch_question(request->query)){
  if(fetch_launch(time(NULL),reply)){
   reply->status=RESEARCH_EVIDENCE;
   audit(request->query,"structured-evidence",reply->sources);
   return STNLABZ_MODULE_OK;
  }
 }
 if(research_general_answer(request->query,host,reply->evidence,
                           sizeof reply->evidence,&reply->sources)){
  reply->status=RESEARCH_EVIDENCE;
  audit(request->query,"general-evidence",reply->sources);
 }else{
  reply->status=RESEARCH_UNAVAILABLE;
  reply->sources=0;
  snprintf(reply->evidence,sizeof reply->evidence,
   "I couldn't verify that right now.");
  audit(request->query,"unverified",0);
 }
 return STNLABZ_MODULE_OK;
}
#ifndef RESEARCH_TEST
static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 research_response_t result;char message[380];
 (void)unused;
 if(!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!cmd->arguments[0])
  return reply(context,"Usage: /research <question>")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 memset(&result,0,sizeof result);
 if(forbidden(cmd->arguments)){
  result.status=RESEARCH_DENIED;
  snprintf(result.evidence,sizeof result.evidence,
   "I can't help with that request.");
 }else if(!permitted_rate()){
  result.status=RESEARCH_UNAVAILABLE;
  snprintf(result.evidence,sizeof result.evidence,
   "I couldn't verify that right now.");
 }else if(research_general_answer(cmd->arguments,host,result.evidence,
                                  sizeof result.evidence,&result.sources)){
  result.status=RESEARCH_EVIDENCE;
 }else{
  result.status=RESEARCH_UNAVAILABLE;
  snprintf(result.evidence,sizeof result.evidence,
   "I couldn't verify that right now.");
 }
 snprintf(message,sizeof message,"%.350s",result.evidence);
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
static const stnlabz_module_descriptor_t descriptor={
 /* AI version progression: 1.4.0 -> 1.4.1 (approved revision arithmetic). */
 /* AI Learn that a modules descriptor belongs at the top of the modules primary .c file, not at the bottom */
 "research","Digit Live Research",1,4,3,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
#endif
