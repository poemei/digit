#define _POSIX_C_SOURCE 200809L
/* Manual integration test. Never linked into the automatic S2 worker. */
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/s2_publish.c"

static int decode_html(const char *src,size_t len,char *dst,size_t cap)
{
 size_t i=0,j=0;
 while(i<len){
  if(j+2>=cap)return 0;
  if(src[i]=='&'){
   static const struct {const char *name;char value;} entities[]={
    {"&amp;",'&'},{"&lt;",'<'},{"&gt;",'>'},{"&quot;",'"'},
    {"&#039;",39},{"&#39;",39},{"&apos;",39}
   };
   size_t k;int matched=0;
   for(k=0;k<sizeof entities/sizeof *entities;++k){
    size_t n=strlen(entities[k].name);
    if(n<=len-i&&!memcmp(src+i,entities[k].name,n)){
     dst[j++]=entities[k].value;i+=n;matched=1;break;
    }
   }
   if(!matched)return 0; /* unknown entity: refuse to rewrite content */
  }else dst[j++]=src[i++];
 }
 dst[j]=0;return 1;
}
static int page_content(const body_t *page,char *dst,size_t cap)
{
 const char *p=strstr(page->data,"id=\"page-content\"");
 const char *start,*end;
 if(!p)return 0;
 start=strchr(p,'>');if(!start)return 0;
 start++;
 end=strstr(start,"</textarea>");
 if(!end)return 0;
 return decode_html(start,(size_t)(end-start),dst,cap);
}
static int form_post(CURL *curl,body_t *reply,const char *token,
                     const char *content,const char *title,
                     const char *description)
{
 char *form;size_t used=0,cap=strlen(content)*3U+4096U;
 CURLcode rc;long code=0;int ok=0;
 form=calloc(cap,1);if(!form)return 0;
 if(!append_form(curl,form,cap,&used,"csrf_token",token)||
    !append_form(curl,form,cap,&used,"action","update")||
    !append_form(curl,form,cap,&used,"original_slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"title",title)||
    !append_form(curl,form,cap,&used,"description",description)||
    !append_form(curl,form,cap,&used,"status","published")||
    !append_form(curl,form,cap,&used,"content_file","main.md")||
    !append_form(curl,form,cap,&used,"content",content))goto done;
 memset(reply,0,sizeof *reply);
 curl_easy_setopt(curl,CURLOPT_URL,S2_SITE "/admin/page");
 curl_easy_setopt(curl,CURLOPT_HTTPGET,0L);
 curl_easy_setopt(curl,CURLOPT_POST,1L);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,form);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(form));
 rc=curl_easy_perform(curl);
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
 ok=(rc==CURLE_OK&&(code==302L||code==303L));
done:
 memset(form,0,cap);free(form);return ok;
}
static int field(const char *html,const char *id,char *dst,size_t cap)
{
 char needle[96];const char *p,*v,*e;size_t n;
 if(snprintf(needle,sizeof needle,"id=\"%s\"",id)>=(int)sizeof needle)return 0;
 p=strstr(html,needle);if(!p)return 0;
 v=strstr(p,"value=\"");if(!v||v>strchr(p,'>'))return 0;
 v+=7;e=strchr(v,'"');if(!e)return 0;
 n=(size_t)(e-v);return decode_html(v,n,dst,cap);
}
int main(int argc,char **argv)
{
 CURL *curl=NULL;body_t *page=NULL;
 char user[128]={0},pass[512]={0},token[256],title[256],description[2048];
 char *original=NULL,*changed=NULL,*verified=NULL,stamp[160];
 size_t capacity=128U*1024U;long code=0;int result=1;
 time_t now=time(NULL);
 if(argc!=2||strcmp(argv[1],"--confirm-live-sai-dsa-update")){
  fputs("Usage: test_sai_dsa_live --confirm-live-sai-dsa-update\n"
        "This makes ONE visible append-only test edit to the live sai-dsa page.\n",stderr);
  return 2;
 }
 page=malloc(sizeof *page);
 original=calloc(capacity,1);changed=calloc(capacity,1);verified=calloc(capacity,1);
 if(!page||!original||!changed||!verified)goto done;
 if(!credentials(user,sizeof user,pass,sizeof pass)){
  fputs("FAIL: credential file inaccessible or not private\n",stderr);goto done;
 }
 fprintf(stderr,"PASS: credentials accessible (values not displayed)\n");
 curl=curl_easy_init();if(!curl)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,25L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,sink);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,page);
 if(!request(curl,S2_SITE "/auth/login",NULL,page)||!csrf(page,token,sizeof token)){
  fputs("FAIL: login form or CSRF token unavailable\n",stderr);goto done;
 }
 fprintf(stderr,"PASS: login form found\n");
 {
  char form[2048]={0};size_t used=0;CURLcode rc;
  if(!append_form(curl,form,sizeof form,&used,"csrf_token",token)||
     !append_form(curl,form,sizeof form,&used,"username",user)||
     !append_form(curl,form,sizeof form,&used,"password",pass))goto done;
  memset(page,0,sizeof *page);
  curl_easy_setopt(curl,CURLOPT_URL,S2_SITE "/auth/login");
  curl_easy_setopt(curl,CURLOPT_POST,1L);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,form);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(form));
  rc=curl_easy_perform(curl);
  curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
  memset(form,0,sizeof form);
  if(rc!=CURLE_OK||(code!=302L&&code!=303L)){
   fprintf(stderr,"FAIL: login HTTP=%ld\n",code);goto done;
  }
 }
 memset(pass,0,sizeof pass);
 fprintf(stderr,"PASS: website login accepted\n");
 if(!request(curl,S2_SITE "/admin/page?edit=sai-dsa",NULL,page)||
    !strstr(page->data,"name=\"original_slug\"")||
    !csrf(page,token,sizeof token)||
    !page_content(page,original,capacity)||
    !field(page->data,"page-title",title,sizeof title)){
  fputs("FAIL: sai-dsa admin edit page not available or cannot be parsed\n",stderr);goto done;
 }
 /* Preserve description if available; do not replace site metadata with test defaults. */
 {
  const char *p=strstr(page->data,"id=\"page-description\"");
  const char *b=p?strchr(p,'>'):NULL,*e=b?strstr(b+1,"</textarea>"):NULL;
  if(!b||!e||!decode_html(b+1,(size_t)(e-b-1),description,sizeof description)){
   fputs("FAIL: page description cannot be preserved\n",stderr);goto done;
  }
 }
 fprintf(stderr,"PASS: existing sai-dsa content and metadata retrieved\n");
 if(snprintf(stamp,sizeof stamp,"\n\n*S2 publishing connectivity test — %ld UTC; verified page access.*\n",
             (long)now)>=(int)sizeof stamp)goto done;
 if(strlen(original)+strlen(stamp)+1U>=capacity)goto done;
 snprintf(changed,capacity,"%s%s",original,stamp);
 if(!form_post(curl,page,token,changed,title,description)){
  fputs("FAIL: page update was rejected; original content should remain\n",stderr);goto done;
 }
 fprintf(stderr,"PASS: page update POST accepted; verifying saved content\n");
 if(!request(curl,S2_SITE "/admin/page?edit=sai-dsa",NULL,page)||
    !page_content(page,verified,capacity)||strcmp(changed,verified)){
  fputs("FAIL: read-back content does not match attempted edit\n",stderr);goto done;
 }
 fprintf(stderr,"PASS: sai-dsa updated and read back exactly; test marker visible at bottom\n");
 result=0;
done:
 memset(pass,0,sizeof pass);
 if(curl)curl_easy_cleanup(curl);
 free(page);free(original);free(changed);free(verified);
 return result;
}
