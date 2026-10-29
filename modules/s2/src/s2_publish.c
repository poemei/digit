#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "s2_public.h"

#define S2_AUTH_FILE "/opt/digit/auth/sites/login.conf"
#define S2_SITE "https://stn-labz.com"
#define S2_BODY_LIMIT (256U*1024U)
typedef struct {char data[S2_BODY_LIMIT];size_t length;} body_t;
static size_t sink(char *data,size_t a,size_t b,void *context)
{
 body_t *r=context;
 size_t n;
 if(!r||!a||b>SIZE_MAX/a)return 0;
 n=a*b;
 if(n>=sizeof r->data-r->length)return 0;
 memcpy(r->data+r->length,data,n);
 r->length+=n;r->data[r->length]=0;
 return n;
}
static int credentials(char *user,size_t user_cap,char *pass,size_t pass_cap)
{
 struct stat st;
 FILE *f;
 char line[1024];
 unsigned mask=0U;
 if(stat(S2_AUTH_FILE,&st)!=0||!S_ISREG(st.st_mode)||
    (st.st_mode&077)!=0|| (st.st_uid!=geteuid()&&st.st_uid!=0))return 0;
 f=fopen(S2_AUTH_FILE,"r");
 if(!f)return 0;
 user[0]=0;pass[0]=0;
 while(fgets(line,sizeof line,f)){
  char *value,*end;
  size_t n=strlen(line);
  if(!n||(!strchr(line,'\n')&&!feof(f)))goto fail;
  end=line+n;
  while(end>line&&(end[-1]=='\n'||end[-1]=='\r')) *--end=0;
  if(line[0]=='#'||line[0]==0)continue;
  value=strchr(line,'=');
  if(!value)goto fail;
  *value++=0;
  if(!strcmp(line,"username")){
   if(mask&1U||!strlen(value)||strlen(value)>=user_cap)goto fail;
   memcpy(user,value,strlen(value)+1U);mask|=1U;
  }else if(!strcmp(line,"password")){
   if(mask&2U||!strlen(value)||strlen(value)>=pass_cap)goto fail;
   memcpy(pass,value,strlen(value)+1U);mask|=2U;
  }else goto fail;
 }
 if(ferror(f)||fclose(f)!=0)return 0;
 return mask==3U;
fail:
 fclose(f);memset(pass,0,pass_cap);
 return 0;
}
static int csrf(const body_t *page,char *token,size_t capacity)
{
 const char *p=page->data,*n,*v,*end;
 size_t len;
 /* Existing site emits csrf_field() as a conventional hidden input. */
 while((p=strstr(p,"<input"))!=NULL){
  const char *close=strchr(p,'>');
  if(!close)break;
  n=strstr(p,"name=\"csrf_token\"");
  if(!n||n>close){p=close+1;continue;}
  v=strstr(p,"value=\"");
  if(!v||v>close){p=close+1;continue;}
  v+=7;end=strchr(v,'"');
  if(!end||end>close){p=close+1;continue;}
  len=(size_t)(end-v);
  if(!len||len>=capacity)return 0;
  memcpy(token,v,len);token[len]=0;
  return 1;
 }
 return 0;
}
static int request(CURL *curl,const char *url,const char *post,body_t *result)
{
 CURLcode rc;long status=0;
 memset(result,0,sizeof *result);
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_HTTPGET,post?0L:1L);
 curl_easy_setopt(curl,CURLOPT_POST,post?1L:0L);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,post?(long)strlen(post):0L);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,result);
 rc=curl_easy_perform(curl);
 if(rc!=CURLE_OK)return 0;
 if(curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status)!=CURLE_OK)return 0;
 return status==200L;
}
static int append_form(CURL *curl,char *out,size_t cap,size_t *used,const char *key,const char *value)
{
 char *encoded=curl_easy_escape(curl,value,0);
 int written;
 if(!encoded)return 0;
 written=snprintf(out+*used,cap-*used,"%s%s=%s",*used?"&":"",key,encoded);
 curl_free(encoded);
 if(written<0||(size_t)written>=cap-*used)return 0;
 *used+=(size_t)written;
 return 1;
}
int s2_public_publish(const char *report)
{
 CURL *curl=NULL;
 body_t *page=NULL;
 char user[128],pass[512],token[256],*form=NULL;
 size_t used=0U,cap;
 int ok=0;
 if(!report||strlen(report)<400U||strlen(report)>60000U||
    !strstr(report,"# STN-LABZ | SECURITY INTELLIGENCE")||
    !strstr(report,"## BUSINESS / OPERATIONAL EFFECTS"))return 0;
 if(!credentials(user,sizeof user,pass,sizeof pass))return 0;
 page=malloc(sizeof *page);
 curl=curl_easy_init();
 if(!curl||!page)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,25L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,sink);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-S2/1.1");
 if(!request(curl,S2_SITE "/auth/login",NULL,page)||!csrf(page,token,sizeof token))goto done;
 cap=2048U;form=calloc(cap,1);
 if(!form)goto done;
 if(!append_form(curl,form,cap,&used,"csrf_token",token)||
    !append_form(curl,form,cap,&used,"username",user)||
    !append_form(curl,form,cap,&used,"password",pass))goto done;
 memset(pass,0,sizeof pass);
 if(!request(curl,S2_SITE "/auth/login",form,page)){
  /* successful login is expected to redirect, not return HTTP 200. */
  long code=0;
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
  if(code!=302L&&code!=303L)goto done;
 }
 memset(form,0,cap);free(form);form=NULL;
 if(!request(curl,S2_SITE "/admin/page?edit=sai-dsa",NULL,page)||
    !strstr(page->data,"name=\"original_slug\"")||
    !strstr(page->data,"sai-dsa")||
    !csrf(page,token,sizeof token))goto done;
 cap=strlen(report)*3U+4096U;
 form=calloc(cap,1);
 if(!form)goto done;
 used=0U;
 if(!append_form(curl,form,cap,&used,"csrf_token",token)||
    !append_form(curl,form,cap,&used,"action","update")||
    !append_form(curl,form,cap,&used,"original_slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"title","sai-dsa")||
    !append_form(curl,form,cap,&used,"status","published")||
    !append_form(curl,form,cap,&used,"content_file","main.md")||
    !append_form(curl,form,cap,&used,"description","STN-LABZ S2 public security intelligence brief.")||
    !append_form(curl,form,cap,&used,"content",report))goto done;
 /* Only accept the normal post-update redirect. */
 {
  long code=0;
  memset(page,0,sizeof *page);
  curl_easy_setopt(curl,CURLOPT_URL,S2_SITE "/admin/page");
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,form);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(form));
  if(curl_easy_perform(curl)!=CURLE_OK)goto done;
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
  if(code!=302L&&code!=303L)goto done;
 }
 ok=1;
done:
 memset(pass,0,sizeof pass);
 if(form){memset(form,0,cap);free(form);}
 if(curl)curl_easy_cleanup(curl);
 free(page);
 return ok;
}
