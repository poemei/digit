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
#include <time.h>
#include "research_engine.h"

#define RESEARCH_SEARCH "https://lite.duckduckgo.com/lite/"
#define RESEARCH_HTTP_LIMIT (4U * 1024U * 1024U)
#define RESEARCH_TEXT_LIMIT (512U * 1024U)
#define RESEARCH_MAX_SEARCHES 3U
#define RESEARCH_MAX_READS 4U

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
 /* Report the transport failure without logging request URLs or content.
  * HTTP, TLS, DNS and connection failures require different repairs. */
 if(!ok)
  research_runtime_log("HTTP","result=%d http=%ld bytes=%zu limit=%zu",
          (int)result,status,document.length,document.max);
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
 const unsigned char *p;
 if(!question||!query||cap<16)return 0;
 p=(const unsigned char *)question;
 /* Ignore an addressed bot name preceding an interrogative. */
 {
  const char *comma=strchr(question,',');
  if(comma&&comma>question&&comma-question<48){
   const unsigned char *after=(const unsigned char *)(comma+1);
   static const char *const starters[]={"when","what","where","who","why","how","which",NULL};
   while(*after&&isspace(*after))after++;
   for(size_t i=0;starters[i];i++){
    size_t n=strlen(starters[i]),j=0;
    while(j<n&&after[j]&&tolower(after[j])==starters[i][j])j++;
    if(j==n&&isspace(after[j])){p=after;break;}
   }
  }
 }
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
 (void)removed;
 return 1;
}
/* Only the addressed name is transport metadata. Submit the user's
 * own question verbatim after removing that prefix; no AI-added terms. */
static int user_search_query(const char *question,char *out,size_t cap)
{
 const char *p=question,*comma,*last;
 size_t n;
 if(!question||!out||cap<2)return 0;
 comma=strchr(question,',');
 if(comma&&comma>question&&comma-question<48){
  const char *after=comma+1;
  static const char *const openers[]={
   "when","what","where","who","why","how","which",NULL
  };
  while(*after&&isspace((unsigned char)*after))after++;
  for(size_t i=0;openers[i];i++){
   size_t j=0,k=strlen(openers[i]);
   while(j<k&&after[j]&&tolower((unsigned char)after[j])==openers[i][j])j++;
   if(j==k&&isspace((unsigned char)after[j])){p=after;break;}
  }
 }
 while(*p&&isspace((unsigned char)*p))p++;
 last=p+strlen(p);
 while(last>p&&isspace((unsigned char)last[-1]))last--;
 n=(size_t)(last-p);
 if(!n||n>=cap)return 0;
 memcpy(out,p,n);out[n]=0;
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
 char terms[256],compound[1100],word[48],seen[256]={0};
 const char *p;
 /* [AI:GPT-6 | 2026-10-31 16:39:00 UTC] */
 unsigned matches=0,unique=0,distinctive=0;
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
  /* Repeated query words are not independent relevance evidence. */
  if(has_word(seen,word))continue;
  if(!append_word(seen,sizeof seen,word))return 0;
  unique++;
  if(has_word(compound,word)){
   matches++;
   /* A single distinctive subject word repeated in the headline
    * and excerpt can identify a relevant abbreviated result. */
   if(i>=7&&has_word(hit->title,word)&&has_word(hit->excerpt,word))
    distinctive=1;
  }
 }
 if(!unique||!matches)return 0;
 if(unique>1&&matches<2&&!distinctive)return 0;
 return matches*10+(!strncmp(hit->url,"https://",8)?1:0);
 /* [End AI:GPT-6] */
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
 /* Accept ISO calendar dates as well as written month/day expressions. */
 for(const char *p=text;p&&*p;p++){
  if(strlen(p)<10)break;
  if(isdigit((unsigned char)p[0])&&isdigit((unsigned char)p[1])&&
     isdigit((unsigned char)p[2])&&isdigit((unsigned char)p[3])&&
     p[4]=='-'&&isdigit((unsigned char)p[5])&&
     isdigit((unsigned char)p[6])&&p[7]=='-'&&
     isdigit((unsigned char)p[8])&&isdigit((unsigned char)p[9]))
   return 1;
 }
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
/* A dated schedule event must be in the future and include a kickoff
 * time. Navigation years and historical results are not event evidence. */
static int next_calendar_event(const char *text,char *out,size_t cap)
{
 static const char *const months[]={
  "jan","feb","mar","apr","may","jun",
  "jul","aug","sep","oct","nov","dec"
 };
 time_t now=time(NULL);struct tm utc={0};
 int today,best=99999999;
 const char *start=text;
 if(!text||!out||cap<64||now==(time_t)-1||!gmtime_r(&now,&utc))return 0;
 today=(utc.tm_year+1900)*10000+(utc.tm_mon+1)*100+utc.tm_mday;
 for(const char *p=start;*p;p++){
  int month=0,day=0,year=utc.tm_year+1900,valid=0;
  size_t consumed=0;
  if((p==start||!isalnum((unsigned char)p[-1]))&&
     isalpha((unsigned char)p[0])&&p[1]&&p[2]&&p[3]&&p[4]){
   for(int m=0;m<12;m++){
    size_t j=0;
    while(j<3&&tolower((unsigned char)p[j])==months[m][j])j++;
    if(j!=3)continue;
    const char *q=p+3;
    while(isalpha((unsigned char)*q))q++;
    if(!isspace((unsigned char)*q))continue;
    while(isspace((unsigned char)*q))q++;
    if(!isdigit((unsigned char)*q))continue;
    day=*q++-'0';
    if(isdigit((unsigned char)*q))day=day*10+(*q++-'0');
    month=m+1;consumed=(size_t)(q-p);valid=1;break;
   }
  }
  if(!valid&&isdigit((unsigned char)*p)&&
     (p==start||!isdigit((unsigned char)p[-1]))){
   const char *q=p;int v=*q++-'0';
   if(isdigit((unsigned char)*q))v=v*10+(*q++-'0');
   if(*q=='/'){
    q++;if(isdigit((unsigned char)*q)){
     day=*q++-'0';
     if(isdigit((unsigned char)*q))day=day*10+(*q++-'0');
     month=v;consumed=(size_t)(q-p);valid=1;
    }
   }
  }
  if(!valid||month<1||month>12||day<1||day>31)continue;
  if(month==1&&utc.tm_mon==11)year++;
  int key=year*10000+month*100+day;
  if(key<today||key>=best||key>today+10000)continue;
  /* The time must be located near this event, not in another game row. */
  const char *end=p+consumed;
  size_t range=0;
  while(end[range]&&range<110)range++;
  char segment[150];size_t left=(size_t)(p-start);
  const char *begin=left>20?p-20:start;
  size_t n=(size_t)(p-begin)+range;
  if(n>sizeof segment-1)n=sizeof segment-1;
  memcpy(segment,begin,n);segment[n]=0;
  if(strstr(segment,"FINAL")||strstr(segment,"Final"))continue;
  if(!strchr(segment,':')||(!strstr(segment," AM")&&!strstr(segment," PM")&&
                         !strstr(segment," ET")&&!strstr(segment," PT")))continue;
  best=key;
  snprintf(out,cap,"%s",segment);
 }
 return best!=99999999;
}
static int schedule_request(const char *question)
{
 char q[512];size_t n;
 if(!question)return 0;
 n=strlen(question);
 if(n>=sizeof q)return 0;
 for(size_t i=0;i<n;i++)
  q[i]=(char)tolower((unsigned char)question[i]);
 q[n]=0;
 return strstr(q,"playing")||strstr(q,"schedule")||
        strstr(q,"next game")||strstr(q,"play again")||
        strstr(q,"game again");
}
/* Extract dated evidence from source pages rather than discarding it.
 * Scheduled WEEK rows are recognized by their published event date/time.
 * All other subjects retain generic, question-matching dated contexts. */
static int scheduled_row(const char *text,char *out,size_t cap)
{
 time_t now=time(NULL);struct tm utc={0};
 const char *p=text;int best=99999999,year,month,day;
 if(now==(time_t)-1||!gmtime_r(&now,&utc))return 0;
 year=utc.tm_year+1900;month=utc.tm_mon+1;day=utc.tm_mday;
 while((p=strstr(p,"WEEK "))!=NULL){
  const char *next=strstr(p+5,"WEEK ");
  size_t n=next?(size_t)(next-p):strlen(p);
  char piece[301];
  if(n>300)n=300;
  memcpy(piece,p,n);piece[n]=0;
  if(!strstr(piece,"FINAL")&&!strstr(piece,"TBD")){
   for(size_t i=0;i+4<n;i++){
    if(isdigit((unsigned char)piece[i])&&
       isdigit((unsigned char)piece[i+1])&&piece[i+2]=='/'&&
       isdigit((unsigned char)piece[i+3])&&
       isdigit((unsigned char)piece[i+4])){
     int mm=(piece[i]-'0')*10+piece[i+1]-'0';
     int dd=(piece[i+3]-'0')*10+piece[i+4]-'0';
     int event_year=year+(month==12&&mm==1?1:0);
     int key=event_year*10000+mm*100+dd;
     if(mm>=1&&mm<=12&&dd>=1&&dd<=31&&
        key>=year*10000+month*100+day&&key<best&&
        strchr(piece,':')&&
        (strstr(piece," AM")||strstr(piece," PM"))){
      best=key;snprintf(out,cap,"%.220s",piece);
     }
     break;
    }
   }
  }
  p+=5;
 }
 return best!=99999999;
}
int research_extract_page_fact(const char *text,const char *question,
                               char *excerpt,size_t cap)
{
 char terms[300],word[48];unsigned best=0;
 if(!text||!question||!excerpt||cap<64)return 0;
 excerpt[0]=0;
 if(!research_terms(question,terms,sizeof terms))return 0;
 if(temporal_question(question)&&schedule_request(question)){
  if(scheduled_row(text,excerpt,cap))return 1;
  if(next_calendar_event(text,excerpt,cap))return 1;
  return 0;
 }
 for(size_t offset=0,len=strlen(text);offset<len;offset+=220){
  char window[481];size_t take=len-offset;unsigned matches=0;
  const char *p=terms;
  if(take>480)take=480;
  memcpy(window,text+offset,take);window[take]=0;
  if(!date_evidence(window))continue;
  while(*p){
   size_t n=0;
   while(*p==' ')p++;
   if(!*p)break;
   while(p[n]&&p[n]!=' '&&n+1<sizeof word){word[n]=p[n];n++;}
   word[n]=0;p+=n;
   if(n>=3&&has_word(window,word))matches++;
  }
  if(matches>best){
   best=matches;
   snprintf(excerpt,cap,"%.*s",(int)(take<cap-1?take:cap-1),window);
  }
 }
 return best>0;
}

/* Research must not discard acquired evidence because a snippet is
 * short or lacks the date asked for. An answer and a source lead are
 * different outcomes; label the source lead and its uncertainty. */
int research_choose_fact(const char *question,const research_hits_t *hits,
                         char *answer,size_t cap,unsigned *sources)
{
 int temporal=0,selected=-1,qualified=-1;
 unsigned best=0,qualified_score=0;
 if(!question||!hits||!answer||cap<64)return 0;
 answer[0]=0;
 if(sources)*sources=0;
 temporal=temporal_question(question);
 for(unsigned i=0;i<hits->count&&i<6;i++){
  const research_hit_t *h=&hits->hit[i];
  int has_date;
  if(!h->title[0]&&!h->excerpt[0])continue;
  if(h->url[0]&&strncmp(h->url,"https://",8))continue;
  has_date=date_evidence(h->excerpt)||date_evidence(h->title)||
           date_evidence(h->date);
  research_runtime_log("HIT",
   "index=%u score=%u date=%d title=%.160s excerpt=%.260s url=%.450s",
   i,h->relevance,has_date,h->title,h->excerpt,h->url);
  if(selected<0||h->relevance>best){selected=(int)i;best=h->relevance;}
  if(h->excerpt[0]&&(!temporal||has_date)&&
     (qualified<0||h->relevance>qualified_score)){
   if(temporal&&schedule_request(question)){
    char event[256]={0};
    if(!scheduled_row(h->excerpt,event,sizeof event)&&
       !next_calendar_event(h->excerpt,event,sizeof event))
     continue;
   }
   qualified=(int)i;qualified_score=h->relevance;
  }
 }
 if(selected<0)return 0;
 /* A directory page or historical list is not an answer to the next
  * scheduled event. Do not promote it to an evidence status. */
 if(temporal&&schedule_request(question)&&qualified<0){
  research_runtime_log("ANSWER_DETAIL",
   "outcome=no_upcoming_event hits=%u",hits->count);
  return 0;
 }
 if(sources)*sources=hits->count;
 if(qualified>=0){
  const research_hit_t *h=&hits->hit[qualified];
  /* Report the text as the source's statement, never as a newly
   * verified conclusion independently established by Digit. */
  snprintf(answer,cap,"Source reports: %.260s",h->excerpt);
  return 1;
 }
 {
  const research_hit_t *h=&hits->hit[selected];
  size_t n;
  snprintf(answer,cap,"%s %.110s%s%.145s",
   temporal?"Date not confirmed in retrieved results. Relevant source:":
            "Relevant source:",
   h->title,h->excerpt[0]?" — ":"",h->excerpt);
  n=strlen(answer);
  if(h->url[0]&&strlen(h->url)+10<cap-n)
   snprintf(answer+n,cap-n," Source: %s",h->url);
  research_runtime_log("ANSWER_DETAIL",
   "outcome=source_lead index=%d temporal=%d",selected,temporal);
  return 1;
 }
}

/* [AI:GPT-6 | 2026-10-31] General-purpose HTML search result parser.
 * A result is evidence only when its title/snippet matches the user's
 * subject. The third-party HTML is untrusted; never execute its contents. */
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

unsigned research_parse_web_html(const char *html,const char *question,
                                 research_hits_t *hits)
{
 const char *cursor,*end;
 if(!html||!question||!hits)return 0;
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
  hit.relevance=score(question,&hit);
  if(!hit.relevance)continue;
  hits->hit[hits->count++]=hit;
 }
 return hits->count;
}
/* [End AI:GPT-6] */

static int search_once(const char *terms,const char *question,
                       research_hits_t *hits)
{
 CURL *curl=curl_easy_init();
 char *url=NULL,*body=NULL;
 char address[1600];
 size_t length=0;
 int ok=0;
 if(!curl)return -1;
 url=curl_easy_escape(curl,terms,0);
 if(!url){
  research_runtime_log("SEARCH_BUILD","outcome=encoding_failed");
  goto done;
 }
 if(snprintf(address,sizeof address,
     RESEARCH_SEARCH "?q=%s",url)>=(int)sizeof address){
  research_runtime_log("SEARCH_BUILD","outcome=url_too_long");
  goto done;
 }
 /* Operator diagnostic: exact constructed search and exact HTTPS URL.
  * URL contains only locally encoded search terms; no credentials. */
 research_runtime_log("SEARCH_QUERY","terms=%s",terms);
 research_runtime_log("SEARCH_REQUEST","url=%s",address);
 body=calloc(RESEARCH_HTTP_LIMIT+1,1);
 if(!body)goto done;
 if(!https_get(address,body,RESEARCH_HTTP_LIMIT+1,&length)){
  research_runtime_log("SEARCH_FETCH","outcome=failed");
  ok=-1;goto done;
 }
 ok=research_parse_web_html(body,question,hits)>0;
 if(!ok)
  research_runtime_log("WEB_FILTER","outcome=empty_or_rejected bytes=%zu links=%d",
          length,strstr(body,"result-link")!=NULL||strstr(body,"result__a")!=NULL);
done:
 if(url)curl_free(url);
 curl_easy_cleanup(curl);
 free(body);
 return ok;
}
static unsigned hit_quality(const research_hit_t *hit)
{
 return hit->relevance+
  (date_evidence(hit->excerpt)||date_evidence(hit->title)?200U:0U)+
  (strlen(hit->excerpt)>60?10U:0U);
}
static int combine_hits(research_hits_t *dest,const research_hits_t *source)
{
 int added=0;
 for(unsigned i=0;i<source->count;i++){
  const research_hit_t *candidate=&source->hit[i];
  int duplicate=-1,weakest=-1;unsigned low=0;
  for(unsigned j=0;j<dest->count;j++){
   unsigned q=hit_quality(&dest->hit[j]);
   if(!strcmp(candidate->url,dest->hit[j].url)){duplicate=(int)j;break;}
   if(weakest<0||q<low){weakest=(int)j;low=q;}
  }
  if(duplicate>=0){
   if(hit_quality(candidate)>hit_quality(&dest->hit[duplicate]))
    dest->hit[duplicate]=*candidate;
  }else if(dest->count<6){dest->hit[dest->count++]=*candidate;added++;}
  else if(weakest>=0&&hit_quality(candidate)>low){
   dest->hit[weakest]=*candidate;added++;
  }
 }
 return added;
}
int research_general_answer(const char *question,const stnlabz_module_host_t *host,
                            char *answer,size_t cap,unsigned *sources)
{
 research_hits_t combined={0},batch={0};
 char query[512];
 unsigned provider_failures=0,attempts=0;
 (void)host;
 if(answer&&cap)answer[0]=0;
 if(sources)*sources=0;
 if(!question||!answer||cap<64||
    !user_search_query(question,query,sizeof query))return 0;
 memset(&batch,0,sizeof batch);
 attempts=1;
 {
  int result=search_once(query,question,&batch);
  if(result>0)combine_hits(&combined,&batch);
  else if(result<0)provider_failures++;
 }
 if(!combined.count){
  research_runtime_log("SEARCH","outcome=no_accepted_hits attempts=%u provider_failures=%u",
          attempts,provider_failures);
  snprintf(answer,cap,"%s",provider_failures==attempts?
   "Research could not reach the search provider.":
   "Search completed but returned no relevant sources.");
  return 0;
 }
 research_runtime_log("SEARCH","outcome=accepted hits=%u",combined.count);
 unsigned reads=0;
 for(unsigned i=0;i<combined.count&&reads<RESEARCH_MAX_READS;i++){
  char *body,*plain;
  size_t received=0;
  if(date_evidence(combined.hit[i].excerpt))continue;
  reads++;
  body=calloc(RESEARCH_HTTP_LIMIT+1,1);
  plain=calloc(RESEARCH_TEXT_LIMIT+1,1);
  if(!body||!plain){free(body);free(plain);continue;}
  if(https_get(combined.hit[i].url,body,RESEARCH_HTTP_LIMIT+1,&received)&&
     research_html_text(body,plain,RESEARCH_TEXT_LIMIT+1)){
   if(!date_evidence(combined.hit[i].excerpt)){
    char extracted[650]={0};
    if(research_extract_page_fact(plain,question,extracted,sizeof extracted)){
     snprintf(combined.hit[i].excerpt,sizeof combined.hit[i].excerpt,
              "%s",extracted);
     research_runtime_log("SOURCE_READ",
       "index=%u outcome=dated_context bytes=%zu excerpt=%.300s",
       i,received,extracted);
    }else research_runtime_log("SOURCE_READ",
       "index=%u outcome=no_relevant_dated_context bytes=%zu",i,received);
   }
  }else research_runtime_log("SOURCE_READ",
      "index=%u outcome=unreadable bytes=%zu",i,received);
  free(body);free(plain);
 }
 {
  int chosen=research_choose_fact(question,&combined,answer,cap,sources);
  research_runtime_log("ANSWER","outcome=%s hits=%u temporal=%d",
          chosen?"evidence":"no_qualified_fact",combined.count,
          temporal_question(question));
  if(!chosen)snprintf(answer,cap,"%s",
   "Relevant sources were found, but the requested fact could not be verified.");
  return chosen;
 }
}
