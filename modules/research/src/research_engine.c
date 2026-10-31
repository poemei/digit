#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include "logging.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "research_engine.h"

#define RESEARCH_SEARCH "https://lite.duckduckgo.com/lite/"
#define RESEARCH_HTTP_LIMIT (4U * 1024U * 1024U)

typedef struct {
 char *data;
 size_t length;
 size_t max;
} document_t;

/* Module ABI 1.5 does not expose Core's logger. Append redacted stage
 * events to Digit's existing log, without creating another log file. */
void research_runtime_log(const char *event,const char *fmt,...)
{
 char detail[1800],line[1900],stamp[32];
 struct tm utc; time_t now; va_list ap;
 int fd,n;
 if(!event||!fmt)return;
 va_start(ap,fmt);
 n=vsnprintf(detail,sizeof detail,fmt,ap);
 va_end(ap);
 if(n<0||(size_t)n>=sizeof detail)return;
 now=time(NULL);
 if(now==(time_t)-1||!gmtime_r(&now,&utc)||
    !strftime(stamp,sizeof stamp,"%Y-%m-%dT%H:%M:%SZ",&utc))return;
 n=snprintf(line,sizeof line,"%s RESEARCH %s %s\n",stamp,event,detail);
 if(n<0||(size_t)n>=sizeof line)return;
 fd=open(STNLABZ_LOG_PATH,O_WRONLY|O_APPEND|O_CLOEXEC|O_NOFOLLOW);
 if(fd<0)return;
 {
  size_t offset=0;
  while(offset<(size_t)n){
   ssize_t written=write(fd,line+offset,(size_t)n-offset);
   if(written<0&&errno==EINTR)continue;
   if(written<=0)break;
   offset+=(size_t)written;
  }
 }
 (void)close(fd);
}


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
#ifdef RESEARCH_TEST
extern int research_test_get(const char *,char *,size_t,size_t *);
#endif
static int https_get(const char *url,char *body,size_t capacity,
                     size_t *length)
{
 CURL *curl;
 document_t document;
 CURLcode result;
 long status=0;
 int ok=0;
 if(!safe_url(url)||capacity<2||!body)return 0;
#ifdef RESEARCH_TEST
 return research_test_get(url,body,capacity,length);
#endif
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
 /* Report the transport failure without logging request URLs or content.
  * HTTP, TLS, DNS and connection failures require different repairs. */
 if(!ok)
  research_runtime_log("HTTP","result=%d http=%ld bytes=%zu limit=%zu",
          (int)result,status,document.length,document.max);
 curl_easy_cleanup(curl);
 if(length)*length=document.length;
 return ok;
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
static int result_class(const char *start,const char *end,const char *name)
{
 const char *p=start;
 size_t n=strlen(name);
 while((p=strstr(p,name))&&p<end){
  if((p==start||!isalnum((unsigned char)p[-1]))&&
     p+n<=end&&!isalnum((unsigned char)p[n]))return 1;
  p+=n;
 }
 return 0;
}
static int link_attribute(const char *start,const char *end,
                          char *out,size_t cap)
{
 const char *p=start;
 while((p=strstr(p,"href"))&&p<end){
  const char *q=p+4,*v;
  char quote=0;
  size_t n=0;
  while(q<end&&isspace((unsigned char)*q))q++;
  if(q>=end||*q!='='){p+=4;continue;}
  q++;
  while(q<end&&isspace((unsigned char)*q))q++;
  if(q>=end)return 0;
  if(*q=='"'||*q==39){quote=*q;q++;}
  v=q;
  while(q<end&&(quote?*q!=quote:!isspace((unsigned char)*q)&&*q!='>'))q++;
  if(q<=v)return 0;
  n=(size_t)(q-v);
  if(n>=cap)return 0;
  memcpy(out,v,n);out[n]=0;
  return 1;
 }
 return 0;
}
/* Search-result URLs may be direct HTTPS links or DuckDuckGo /l/
 * redirect links. Decode only the uddg parameter, never the trailing
 * tracking parameters, and never follow the redirect itself. */
static int decode_search_link(const char *raw,char *out,size_t cap)
{
 const char *encoded=NULL,*p;
 size_t n=0;
 if(!raw||!out||cap<16)return 0;
 if(!strncmp(raw,"/l/?",4)||
    !strncmp(raw,"//duckduckgo.com/l/?",20)||
    !strncmp(raw,"https://duckduckgo.com/l/?",26)||
    !strncmp(raw,"https://www.duckduckgo.com/l/?",30)){
  const char *query=strchr(raw,'?');
  if(!query)return 0;
  p=query+1;
  while(*p){
   if(!strncmp(p,"uddg=",5)){encoded=p+5;break;}
   p=strchr(p,'&');
   if(!p)break;
   p++;
   if(!strncmp(p,"amp;",4))p+=4;
  }
  if(!encoded)return 0;
  for(p=encoded;*p&&*p!='&'&&*p!='#';p++){
   unsigned char c=(unsigned char)*p;
   if(c=='%'&&isxdigit((unsigned char)p[1])&&
      p[1]&&p[2]&&isxdigit((unsigned char)p[2])){
    unsigned char a=(unsigned char)tolower((unsigned char)p[1]);
    unsigned char b=(unsigned char)tolower((unsigned char)p[2]);
    a=(unsigned char)(isdigit(a)?a-'0':a-'a'+10);
    b=(unsigned char)(isdigit(b)?b-'0':b-'a'+10);
    c=(unsigned char)(a*16+b);
    p+=2;
   }
   if(c<' '||c==127||n+1>=cap)return 0;
   out[n++]=(char)c;
  }
 }else{
  /* Preserve escapes in ordinary destination URLs. HTML-encoded
   * ampersands in href attributes are the only transformation. */
  for(p=raw;*p;p++){
   if(!strncmp(p,"&amp;",5)){
    if(n+1>=cap)return 0;
    out[n++]='&';p+=4;
    continue;
   }
   if(n+1>=cap)return 0;
   out[n++]=*p;
  }
 }
 out[n]=0;
 return safe_url(out);
}

/* A result snippet often contains <b> or <strong> tags.
 * Extract through the corresponding closing element, not the first '<'. */
static int snippet_text(const char *start,const char *tagend,
                        const char *limit,char *out,size_t cap)
{
 const char *name=start+1,*p=name,*closing;
 char endtag[40];size_t length;
 while(p<tagend&&isalnum((unsigned char)*p))p++;
 length=(size_t)(p-name);
 if(!length||length>20)return 0;
 if(snprintf(endtag,sizeof endtag,"</%.*s>",(int)length,name)
    >=(int)sizeof endtag)return 0;
 closing=strstr(tagend+1,endtag);
 if(!closing||closing>=limit)return 0;
 strip_markup(tagend+1,(size_t)(closing-tagend-1),out,cap);
 return out[0]!=0;
}

unsigned research_parse_web_html(const char *html,
                                 research_hits_t *hits)
{
 const char *cursor,*end;
 if(!html||!hits)return 0;
 memset(hits,0,sizeof *hits);
 cursor=html;end=html+strlen(html);
 for(unsigned scanned=0;scanned<60&&hits->count<6;scanned++){
  const char *a=strstr(cursor,"<a"),*gt,*close,*next;
  research_hit_t hit={0};char link[700];
  if(!a||a>=end)break;
  gt=strchr(a,'>');if(!gt||gt>=end)break;
  cursor=gt+1;
  if(!result_class(a,gt,"result-link")&&
     !result_class(a,gt,"result__a"))continue;
  close=strstr(gt+1,"</a>");
  if(!close||close>end)continue;
  next=close+4;
  if(!link_attribute(a,gt,link,sizeof link)||
     !decode_search_link(link,hit.url,sizeof hit.url))continue;
  strip_markup(gt+1,(size_t)(close-gt-1),hit.title,sizeof hit.title);
  if(!hit.title[0])continue;
  /* DuckDuckGo advertising disclosure links are not organic results. */
  if(strstr(hit.url,"duckduckgo.com/duckduckgo-help-pages/company/ads"))continue;
  /* Match the snippet belonging to this result, not the next result.
   * Both DDG Lite and DDG HTML provide a distinct snippet class. */
  {
   const char *scan=next;
   const char *limit=(size_t)(end-next)<4000?end:next+4000;
   while((scan=strchr(scan,'<'))&&scan<limit){
    const char *tagend=strchr(scan,'>');
    if(!tagend||tagend>=limit)break;
    if(result_class(scan,tagend,"result-link")||
       result_class(scan,tagend,"result__a"))break;
    if(result_class(scan,tagend,"result-snippet")||
       result_class(scan,tagend,"result__snippet")){
     (void)snippet_text(scan,tagend,limit,
                        hit.excerpt,sizeof hit.excerpt);
     break;
    }
    scan=tagend+1;
   }
  }
  int duplicate=0;
  for(unsigned i=0;i<hits->count;i++)
   if(!strcmp(hits->hit[i].url,hit.url)){duplicate=1;break;}
  if(duplicate)continue;
  hits->hit[hits->count++]=hit;
 }
 return hits->count;
}
/* [End AI:GPT-6] */


/*
 * Only next/upcoming WHEN questions need a temporal search hint.
 * All other search strings are passed to DuckDuckGo unchanged.
 * This is grammar-based, not a list of teams, products or events.
 */
static void search_terms(const char *question,char *out,size_t cap)
{
 const char *p=question;
 char base[512],lower[512],date[16];
 size_t n;
 time_t now;struct tm utc;
 int temporal=0;
 snprintf(out,cap,"%s",question);
 if(strncasecmp(p,"when ",5))return;
 for(size_t i=0;question[i]&&i+1<sizeof lower;i++)
  lower[i]=(char)tolower((unsigned char)question[i]);
 lower[strlen(question)]=0;
 temporal=strstr(lower," again")!=NULL||
          strstr(lower," next ")!=NULL||
          strstr(lower," upcoming ")!=NULL;
 if(!temporal)return;
 p+=5;
 const char *aux[]={"are ","is ","will ","does ","do ",NULL};
 for(unsigned i=0;aux[i];i++)if(!strncasecmp(p,aux[i],strlen(aux[i]))){
  p+=strlen(aux[i]);break;
 }
 while(*p==' ')p++;
 if(!strncasecmp(p,"the ",4))p+=4;
 snprintf(base,sizeof base,"%s",p);
 n=strlen(base);
 while(n&&(base[n-1]=='?'||base[n-1]==' '||base[n-1]=='.'))base[--n]=0;
 if(n>=6&&!strncasecmp(base+n-6," again",6))base[n-=6]=0;
 n=strlen(base);
 if(n>=8&&!strncasecmp(base+n-8," playing",8))base[n-=8]=0;
 while(n&&base[n-1]==' ')base[--n]=0;
 if(!base[0])return;
 now=time(NULL);
 if(now==(time_t)-1||!gmtime_r(&now,&utc))return;
 if(!strftime(date,sizeof date,"%Y-%m-%d",&utc))return;
 snprintf(out,cap,"%s next schedule after %s date time location",base,date);
}

/* [AI:Codex | 2026-10-31] Search the supplied text once. No subject
 * rewriting, page-reading or answer qualification belongs in retrieval. */
int research_search(const char *query,research_hits_t *hits)
{
 CURL *curl=NULL;
 char *encoded=NULL,*body=NULL,address[1600],terms[1024];
 size_t length=0;
 int result=-1;
 if(!hits)return -1;
 memset(hits,0,sizeof *hits);
 if(!query||!*query||strlen(query)>=512)return -1;
 curl=curl_easy_init();
 if(!curl)goto done;
 search_terms(query,terms,sizeof terms);
 encoded=curl_easy_escape(curl,terms,0);
 if(!encoded)goto done;
 if(snprintf(address,sizeof address,RESEARCH_SEARCH "?q=%s",encoded)
    >=(int)sizeof address)goto done;
 research_runtime_log("SEARCH_QUERY","terms=%s",query);
 research_runtime_log("SEARCH_REQUEST","url=%s",address);
 body=calloc(RESEARCH_HTTP_LIMIT+1,1);
 if(!body||!https_get(address,body,RESEARCH_HTTP_LIMIT+1,&length))goto done;
 /* A challenge/changed page is not an empty search result. */
 if(strstr(body,"anomaly.js")||strstr(body,"challenge-form"))goto done;
 if(research_parse_web_html(body,hits))result=1;
 else if(strstr(body,"No results found")||strstr(body,"No more results"))result=0;
done:
 research_runtime_log("SEARCH","outcome=%s hits=%u bytes=%zu",
  result>0?"results":result==0?"empty":"provider_or_parse_failure",hits->count,length);
 if(encoded)curl_free(encoded);
 if(curl)curl_easy_cleanup(curl);
 free(body);
 return result;
}
