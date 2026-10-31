#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "research_engine.h"

#define RESEARCH_SEARCH "https://www.bing.com/search"
#define RESEARCH_HTTP_LIMIT (512U * 1024U)
#define RESEARCH_TEXT_LIMIT 1200U
#define RESEARCH_MAX_SEARCHES 3U
#define RESEARCH_MAX_READS 2U

typedef struct {
 char *data;
 size_t length;
 size_t max;
} document_t;

static size_t receive_body(char *ptr,size_t size,size_t count,void *ctx)
{
 document_t *doc=ctx;
 size_t n;
 if(!size||count>SIZE_MAX/size)return 0;
 n=size*count;
 if(n>doc->max-doc->length)return 0;
 memcpy(doc->data+doc->length,ptr,n);
 doc->length+=n;
 doc->data[doc->length]=0;
 return n;
}
static int public_v4(const unsigned char *b)
{
 if(b[0]==0||b[0]==10||b[0]==127||b[0]>=224||
    (b[0]==169&&b[1]==254)||
    (b[0]==172&&b[1]>=16&&b[1]<=31)||
    (b[0]==192&&b[1]==168)||
    (b[0]==100&&b[1]>=64&&b[1]<=127)||
    (b[0]==192&&b[1]==0&&b[2]==0)||
    (b[0]==198&&b[1]>=18&&b[1]<=19)||
    (b[0]==192&&b[1]==0&&b[2]==2)||
    (b[0]==198&&b[1]==51&&b[2]==100)||
    (b[0]==203&&b[1]==0&&b[2]==113))return 0;
 return 1;
}
/* Reject internal/reserved socket targets after DNS resolution, not just
 * the untrusted hostname text. No redirects, HTTP or proxies allowed. */
static curl_socket_t open_public_socket(void *unused,curlsocktype kind,
                                        struct curl_sockaddr *address)
{
 curl_socket_t sock;
 (void)unused;(void)kind;
 if(address->family==AF_INET){
  struct sockaddr_in *sa=(struct sockaddr_in *)&address->addr;
  if(!public_v4((const unsigned char *)&sa->sin_addr))return CURL_SOCKET_BAD;
 }else if(address->family==AF_INET6){
  struct sockaddr_in6 *sa=(struct sockaddr_in6 *)&address->addr;
  const unsigned char *a=(const unsigned char *)&sa->sin6_addr;
  static const unsigned char mapped[12]={0,0,0,0,0,0,0,0,0,0,255,255};
  if(!memcmp(a,mapped,12)){
   if(!public_v4(a+12))return CURL_SOCKET_BAD;
  }else if((a[0]&0xfe)==0xfc||a[0]==0xff||
           (a[0]==0xfe&&(a[1]&0xc0)==0x80)||
           (!memcmp(a,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1",16))||
           (!memcmp(a,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16)))
   return CURL_SOCKET_BAD;
 }else return CURL_SOCKET_BAD;
 sock=socket(address->family,address->socktype,address->protocol);
 return sock;
}
static int safe_url(const char *url)
{
 const char *p,*end;size_t n,alpha=0;
 if(!url||strncmp(url,"https://",8))return 0;
 p=url+8;end=p;
 while(*end&&*end!='/'&&*end!='?'&&*end!='#')end++;
 n=(size_t)(end-p);
 if(n<5||n>200||!memchr(p,'.',n))return 0;
 for(size_t i=0;i<n;i++){
  unsigned char c=(unsigned char)p[i];
  if(isalpha(c))alpha++;
  if(!isalnum(c)&&c!='.'&&c!='-')return 0;
 }
 if(!alpha||strstr(p,"..")==p)return 0;
 return 1;
}
static int https_get(const char *url,char *body,size_t capacity,
                     size_t *length)
{
 CURL *curl;
 document_t document;
 CURLcode result;
 long status=0;
 int ok=0;
 if(!safe_url(url)||capacity<2||!body)return 0;
 curl=curl_easy_init();
 if(!curl)return 0;
 document.data=body;document.length=0;
 document.max=capacity-1;
 body[0]=0;
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,3L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,8L);
 curl_easy_setopt(curl,CURLOPT_OPENSOCKETFUNCTION,open_public_socket);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive_body);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&document);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"DigitResearch/1.4 (+read-only)");
 curl_easy_setopt(curl,CURLOPT_ACCEPT_ENCODING,"");
 result=curl_easy_perform(curl);
 if(result==CURLE_OK){
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
  ok=status==200&&document.length>20;
 }
 curl_easy_cleanup(curl);
 if(length)*length=document.length;
 return ok;
}
static int term_end(unsigned char c){return !isalnum(c)&&c!='+'&&c!='#';}
static int has_word(const char *text,const char *word)
{
 const unsigned char *p=(const unsigned char *)text;
 size_t n=strlen(word);
 if(!n)return 0;
 while(*p){
  size_t i=0;
  if(p!=(const unsigned char *)text&&!term_end(p[-1])){p++;continue;}
  while(i<n&&p[i]&&tolower(p[i])==(unsigned char)word[i])i++;
  if(i==n&&term_end(p[i]))return 1;
  p++;
 }
 return 0;
}
static int stop(const char *w)
{
 static const char *const words[]={
  "a","about","again","all","am","an","and","are","as","ask","at",
  "be","can","could","do","does","for","from","get","give",
  "have","how","i","in","into","is","it","its","know","learn",
  "latest","me","my","next","of","on","or","our","please","show","tell",
  "the","their","then","there","this","to","us","want","what",
  "when","where","which","who","will","with","would","you","your",
  NULL
 };
 for(size_t i=0;words[i];i++)if(!strcmp(w,words[i]))return 1;
 return 0;
}
static int append_word(char *dest,size_t cap,const char *word)
{
 size_t n=strlen(dest),m=strlen(word);
 if(n+m+2>cap)return 0;
 if(n)dest[n++]=' ';
 memcpy(dest+n,word,m+1);
 return 1;
}
/* Generic subject extraction: no product, team, technology or language
 * allowlist. Keep significant words in their original order. */
int research_terms(const char *question,char *query,size_t cap)
{
 size_t count=0,removed=0;
 const unsigned char *p=(const unsigned char *)question;
 if(!question||!query||cap<16)return 0;
 query[0]=0;
 while(*p&&count<14){
  char word[48];size_t n=0;
  while(*p&&term_end(*p))p++;
  while(*p&&!term_end(*p)){
   if(n+1<sizeof word)word[n++]=(char)tolower(*p);
   p++;
  }
  if(!n)continue;
  word[n]=0;
  if((n<2&&strcmp(word,"c"))||stop(word)){removed++;continue;}
  if(!append_word(query,cap,word))break;
  count++;
 }
 if(!count)return 0;
 /* The question supplies intent, while subject terms stay topic-neutral. */
 if((strstr(question,"next")||strstr(question,"Next")||
     strstr(question,"when do")||strstr(question,"When do")||
     strstr(question,"when is")||strstr(question,"When is"))&&count<13)
  (void)append_word(query,cap,"upcoming schedule");
 else if(strstr(question,"learn ")||strstr(question,"Learn ")||
         strstr(question,"study ")||strstr(question,"Study "))
  (void)append_word(query,cap,"official documentation");
 (void)removed;
 return 1;
}
static void strip_markup(const char *src,size_t len,char *out,size_t cap)
{
 size_t i=0,j=0;int markup=0,white=1;
 for(;i<len&&j+1<cap;i++){
  unsigned char c=(unsigned char)src[i];
  if(c=='<'){markup=1;continue;}
  if(markup){if(c=='>')markup=0;continue;}
  if(c=='&'){
   if(i+5<len&&!memcmp(src+i,"&amp;",5)){c='&';i+=4;}
   else if(i+6<len&&!memcmp(src+i,"&quot;",6)){c='"';i+=5;}
   else if(i+4<len&&!memcmp(src+i,"&lt;",4)){c='<';i+=3;}
   else if(i+4<len&&!memcmp(src+i,"&gt;",4)){c='>';i+=3;}
   else if(i+6<len&&!memcmp(src+i,"&#39;",5)){c=39;i+=4;}
   else c=' ';
  }
  if(c<32||c==127)c=' ';
  if(isspace(c)){
   if(!white)out[j++]=' ';
   white=1;
  }else{
   out[j++]=(char)c;white=0;
  }
 }
 while(j>0&&out[j-1]==' ')j--;
 out[j]=0;
}
static int element(const char *start,const char *end,const char *tag,
                   char *dest,size_t cap)
{
 char left[32],right[36];
 const char *a,*b;
 if(snprintf(left,sizeof left,"<%s>",tag)>=(int)sizeof left||
    snprintf(right,sizeof right,"</%s>",tag)>=(int)sizeof right)return 0;
 a=strstr(start,left);
 if(!a||a>=end)return 0;
 a+=strlen(left);b=strstr(a,right);
 if(!b||b>end)return 0;
 strip_markup(a,(size_t)(b-a),dest,cap);
 return dest[0]!=0;
}
static unsigned score(const char *question,const research_hit_t *hit)
{
 char terms[256],compound[1100],word[48];
 const char *p;
 unsigned matches=0,unique=0;
 if(!research_terms(question,terms,sizeof terms))return 0;
 snprintf(compound,sizeof compound,"%s %s",hit->title,hit->excerpt);
 p=terms;
 while(*p){
  size_t i=0;
  while(*p==' ')p++;
  if(!*p)break;
  while(p[i]&&p[i]!=' '&&i+1<sizeof word){word[i]=p[i];i++;}
  word[i]=0;p+=i;
  if(i<2&&strcmp(word,"c")!=0)continue;
  if(unique==0&&!has_word(compound,word))return 0;
  unique++;
  if(has_word(compound,word))matches++;
 }
 if(!unique||!matches)return 0;
 if(unique>1&&matches<2)return 0;
 return matches*10+(!strncmp(hit->url,"https://",8)?1:0);
}
unsigned research_parse_rss(const char *rss,const char *question,
                            research_hits_t *hits)
{
 const char *cursor,*end;
 if(!rss||!question||!hits)return 0;
 memset(hits,0,sizeof *hits);
 if(!strstr(rss,"<rss"))return 0;
 cursor=rss;
 end=rss+strlen(rss);
 for(unsigned examined=0;examined<25&&hits->count<6;examined++){
  const char *a=strstr(cursor,"<item>"),*b;
  research_hit_t h={0};
  if(!a||a>=end)break;
  b=strstr(a,"</item>");
  if(!b||b>=end)break;
  cursor=b+7;
  if(!element(a,b,"title",h.title,sizeof h.title)||
     !element(a,b,"link",h.url,sizeof h.url)||
     !safe_url(h.url))continue;
  (void)element(a,b,"description",h.excerpt,sizeof h.excerpt);
  (void)element(a,b,"pubDate",h.date,sizeof h.date);
  h.relevance=score(question,&h);
  if(h.relevance==0)continue;
  hits->hit[hits->count++]=h;
 }
 return hits->count;
}
/* Simple bounded text extraction without running HTML, scripts, CSS, or
 * following web-page instructions. This is evidence data, not a command. */
int research_html_text(const char *html,char *text,size_t cap)
{
 const char *p=html;
 size_t used=0;
 int block=0;
 if(!html||!text||cap<2)return 0;
 text[0]=0;
 while(*p&&used+1<cap){
  if(*p=='<'){
   char tag[32];size_t n=0;
   const char *start=p;
   p++;
   while(*p&&isspace((unsigned char)*p))p++;
   int closing=(*p=='/');
   if(closing)p++;
   while(*p&&isalpha((unsigned char)*p)&&n+1<sizeof tag)
    tag[n++]=(char)tolower((unsigned char)*p++);
   tag[n]=0;
   if(!strcmp(tag,"script")||!strcmp(tag,"style")||
      !strcmp(tag,"nav")||!strcmp(tag,"footer")){
    if(!closing)block++;
    else if(block)block--;
   }
   p=start;
   while(*p&&*p!='>')p++;
   if(*p)p++;
   if(!block&&used&&text[used-1]!=' ')text[used++]=' ';
   continue;
  }
  if(block){p++;continue;}
  if(*p=='&'){
   if(!strncmp(p,"&nbsp;",6)){p+=6;continue;}
   if(!strncmp(p,"&amp;",5)){p+=5;text[used++]='&';continue;}
   if(!strncmp(p,"&lt;",4)){p+=4;text[used++]='<';continue;}
   if(!strncmp(p,"&gt;",4)){p+=4;text[used++]='>';continue;}
   if(!strncmp(p,"&quot;",6)){p+=6;text[used++]='"';continue;}
  }
  if((unsigned char)*p>=32&&(unsigned char)*p!=127){
   if(isspace((unsigned char)*p)){
    if(used&&text[used-1]!=' ')text[used++]=' ';
   }else text[used++]=*p;
  }
  p++;
 }
 while(used&&text[used-1]==' ')used--;
 text[used]=0;
 return used>40;
}
/* Facts returned to the operator are selected directly from retrieved
 * source text. No other module is required to generate a response. */
int research_grounded(const char *answer,const char *evidence)
{
 size_t n;
 if(!answer||!evidence)return 0;
 n=strlen(answer);
 return n>15&&n<=420&&!strstr(answer,"http://")&&
        !strstr(answer,"https://")&&strstr(evidence,answer)!=NULL;
}
static int temporal_question(const char *q)
{
 const char *markers[]={"when","next","schedule","upcoming","date",
                        "time","today","tomorrow","latest",NULL};
 char normalized[512];size_t n=strlen(q);
 if(n>=sizeof normalized)return 0;
 for(size_t i=0;i<n;i++)normalized[i]=(char)tolower((unsigned char)q[i]);
 normalized[n]=0;
 for(size_t i=0;markers[i];i++)
  if(strstr(normalized,markers[i]))return 1;
 return 0;
}
static int date_evidence(const char *text)
{
 static const char *words[]={"january","february","march","april","may","june",
 "july","august","september","october","november","december",
 "jan","feb","mar","apr","jun","jul","aug","sep","sept","oct","nov","dec",
 "monday","tuesday","wednesday","thursday","friday","saturday","sunday",
 "am","pm","utc","et","est","edt","pt","pst","pdt",NULL};
 int digit=0;
 for(const char *p=text;*p;p++)if(isdigit((unsigned char)*p)){digit=1;break;}
 if(!digit)return 0;
 for(size_t i=0;words[i];i++)if(has_word(text,words[i]))return 1;
 return 0;
}
int research_choose_fact(const char *question,const research_hits_t *hits,
                       char *answer,size_t cap,unsigned *sources)
{
 int temporal=temporal_question(question);
 unsigned best_score=0,best=0;
 char candidate[550],best_text[550]={0};
 if(!hits||!answer||cap<64)return 0;
 for(unsigned i=0;i<hits->count;i++){
  const research_hit_t *h=&hits->hit[i];
  const char *text=h->excerpt;
  unsigned value=h->relevance;
  size_t len;
  if(!text||!*text)continue;
  len=strlen(text);
  if(len<35)continue;
  if(temporal){
   if(!date_evidence(text))continue;
   value+=20;
  }
  /* Avoid promoting an index title as an answer. Select the factual
   * excerpt and preserve its wording rather than inventing details. */
  snprintf(candidate,sizeof candidate,"%.400s",text);
  if(value>best_score){
   best_score=value;best=i;
   snprintf(best_text,sizeof best_text,"%s",candidate);
  }
 }
 if(!best_score||!research_grounded(best_text,hits->hit[best].excerpt))
  return 0;
 if(strlen(best_text)>=cap)return 0;
 strcpy(answer,best_text);
 if(sources)*sources=hits->count;
 return 1;
}
static int search_once(const char *terms,const char *question,
                       research_hits_t *hits)
{
 CURL *curl=curl_easy_init();
 char *url=NULL,*body=NULL;
 char address[1600];
 size_t length=0;
 int ok=0;
 if(!curl)return 0;
 url=curl_easy_escape(curl,terms,0);
 if(!url||snprintf(address,sizeof address,
     RESEARCH_SEARCH "?format=rss&q=%s",url)>=(int)sizeof address)goto done;
 body=calloc(RESEARCH_HTTP_LIMIT+1,1);
 if(!body)goto done;
 if(!https_get(address,body,RESEARCH_HTTP_LIMIT+1,&length))goto done;
 ok=research_parse_rss(body,question,hits)>0;
done:
 if(url)curl_free(url);
 curl_easy_cleanup(curl);
 free(body);
 return ok;
}
static int combine_hits(research_hits_t *dest,const research_hits_t *source)
{
 int added=0;
 for(unsigned i=0;i<source->count&&dest->count<6;i++){
  int duplicate=0;
  for(unsigned j=0;j<dest->count;j++){
   if(!strcmp(source->hit[i].url,dest->hit[j].url)){duplicate=1;break;}
  }
  if(!duplicate){dest->hit[dest->count++]=source->hit[i];added++;}
 }
 return added;
}
static int make_variation(const char *terms,const char *question,
                          unsigned attempt,char *out,size_t cap)
{
 const char *suffix;
 if(strstr(question,"learn")||strstr(question,"study")||
    strstr(question,"documentation")){
  suffix=attempt==1?" official reference":" tutorial examples";
 }else if(strstr(question,"when")||strstr(question,"next")||
          strstr(question,"schedule")){
  suffix=attempt==1?" schedule date time":" upcoming official";
 }else{
  suffix=attempt==1?" primary source":" detailed explanation";
 }
 return snprintf(out,cap,"%s%s",terms,suffix)<(int)cap;
}
int research_general_answer(const char *question,const stnlabz_module_host_t *host,
                            char *answer,size_t cap,unsigned *sources)
{
 research_hits_t combined={0},batch={0};
 char query[300],variant[340];
 (void)host;
 if(answer&&cap)answer[0]=0;
 if(sources)*sources=0;
 if(!question||!answer||cap<64||
    !research_terms(question,query,sizeof query))return 0;
 for(unsigned attempt=0;attempt<RESEARCH_MAX_SEARCHES;attempt++){
  if(attempt==0)snprintf(variant,sizeof variant,"%s",query);
  else if(!make_variation(query,question,attempt,variant,sizeof variant))break;
  memset(&batch,0,sizeof batch);
  if(search_once(variant,question,&batch))combine_hits(&combined,&batch);
  if(combined.count>=3&&attempt>=1)break;
 }
 if(!combined.count)return 0;
 for(unsigned i=0;i<combined.count&&i<RESEARCH_MAX_READS;i++){
  char *body=calloc(RESEARCH_HTTP_LIMIT+1,1);
  char *plain=calloc(RESEARCH_TEXT_LIMIT+1,1);
  size_t received=0;
  if(!body||!plain){free(body);free(plain);continue;}
  if(https_get(combined.hit[i].url,body,RESEARCH_HTTP_LIMIT+1,&received)&&
     research_html_text(body,plain,RESEARCH_TEXT_LIMIT+1)){
   /* Retain the initial excerpt as the default factual candidate.
    * Page content may supplement it if the excerpt lacks an event date. */
   if(!date_evidence(combined.hit[i].excerpt)&&date_evidence(plain)){
    snprintf(combined.hit[i].excerpt,sizeof combined.hit[i].excerpt,
      "%.400s",plain);
   }
  }
  free(body);free(plain);
 }
 return research_choose_fact(question,&combined,answer,cap,sources);
}
