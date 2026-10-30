#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <ctype.h>
#include <stdint.h>
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
 if(!curl||!url||!result)return 0;
 memset(result,0,sizeof *result);
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,result);
 if(post){
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,(long)strlen(post));
  curl_easy_setopt(curl,CURLOPT_POST,1L);
 }else{
  /* CURLOPT_POSTFIELDS, even when set to NULL, can select POST.
   * Make HTTPGET the FINAL method-setting option. */
  curl_easy_setopt(curl,CURLOPT_POSTFIELDS,NULL);
  curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,0L);
  curl_easy_setopt(curl,CURLOPT_HTTPGET,1L);
 }
 rc=curl_easy_perform(curl);
 if(rc==CURLE_OK)(void)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
 if(rc!=CURLE_OK||(post?(status!=302L&&status!=303L):(status!=200L))){
  fprintf(stderr,"[S2] site request stage=%s HTTP=%ld curl=%d bytes=%zu\\n",
          post?"POST":"GET",status,(int)rc,result->length);
  return 0;
 }
 return 1;
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

/* HTML values come from the site editing form, not from a public web page. */
static int html_text(const char *begin,size_t n,char *out,size_t cap)
{
 size_t i=0,j=0;
 while(i<n){
  if(j+1>=cap)return 0;
  if(begin[i]=='&'){
   static const struct {const char *entity;char character;} substitutions[]={
    {"&amp;",'&'},{"&lt;",'<'},{"&gt;",'>'},{"&quot;",'"'},
    {"&#039;",39},{"&#39;",39},{"&apos;",39}
   };
   size_t k;int found=0;
   for(k=0;k<sizeof substitutions/sizeof substitutions[0];++k){
    size_t t=strlen(substitutions[k].entity);
    if(t<=n-i&&!memcmp(begin+i,substitutions[k].entity,t)){
     out[j++]=substitutions[k].character;i+=t;found=1;break;
    }
   }
   if(!found)return 0;
  }else out[j++]=begin[i++];
 }
 out[j]=0;return 1;
}
static int input_value(const body_t *body,const char *id,char *out,size_t cap)
{
 char needle[100];const char *p,*tag,*value,*end,*close;
 if(snprintf(needle,sizeof needle,"id=\"%s\"",id)>=(int)sizeof needle)return 0;
 p=strstr(body->data,needle);if(!p)return 0;
 tag=p;
 while(tag>body->data&&*tag!='<')--tag;
 close=strchr(p,'>');if(!close||strncmp(tag,"<input",6))return 0;
 value=strstr(tag,"value=\"");if(!value||value>=close)return 0;
 value+=7;end=strchr(value,'"');
 if(!end||end>close)return 0;
 return html_text(value,(size_t)(end-value),out,cap);
}
static int textarea_value(const body_t *body,const char *id,char *out,size_t cap)
{
 char needle[100];const char *p,*start,*end,*tag;
 if(snprintf(needle,sizeof needle,"id=\"%s\"",id)>=(int)sizeof needle)return 0;
 p=strstr(body->data,needle);if(!p)return 0;
 tag=p;while(tag>body->data&&*tag!='<')--tag;
 if(strncmp(tag,"<textarea",9))return 0;
 start=strchr(p,'>');if(!start)return 0;
 ++start;end=strstr(start,"</textarea>");
 if(!end)return 0;
 return html_text(start,(size_t)(end-start),out,cap);
}
static int edit_page(const body_t *body,char *csrf_token,size_t csrf_size,
                     char *title,size_t title_size,
                     char *description,size_t description_size,
                     char *content,size_t content_size)
{
 char slug[80];
 /* The test-proven form uses an editable page title and textareas.
  * Never mistake the page-list delete form for the editor. */
 if(!strstr(body->data,"name=\"original_slug\"")||
    !input_value(body,"page-slug",slug,sizeof slug)||
    strcmp(slug,"sai-dsa")||
    !input_value(body,"page-title",title,title_size)||
    !textarea_value(body,"page-description",description,description_size)||
    !textarea_value(body,"page-content",content,content_size)||
    !csrf(body,csrf_token,csrf_size))return 0;
 return title[0]!=0;
}

int s2_public_publish(const char *report)
{
 CURL *curl=NULL;
 body_t *page=NULL;
 char user[128]={0},pass[512]={0},token[256]={0},title[256]={0};
 char description[4096]={0},*previous=NULL,*verified=NULL,*form=NULL;
 size_t used=0,cap=0,limit=128U*1024U;
 int ok=0;
 if(!report||strlen(report)<400U||strlen(report)>60000U||
    !strstr(report,"# STN-LABZ | SECURITY INTELLIGENCE")||
    !strstr(report,"## BUSINESS / OPERATIONAL EFFECTS"))return 0;
 if(!credentials(user,sizeof user,pass,sizeof pass)){
  fputs("[S2] site credentials unavailable to service user\n",stderr);return 0;
 }
 page=calloc(1,sizeof *page);
 previous=calloc(limit,1);verified=calloc(limit,1);
 curl=curl_easy_init();
 if(!curl||!page||!previous||!verified)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,25L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,sink);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-S2/1.2");
 if(!request(curl,S2_SITE "/auth/login",NULL,page)){
  fputs("[S2] login form unavailable\n",stderr);goto done;
 }
 if(!csrf(page,token,sizeof token)){
  fputs("[S2] login CSRF unavailable\n",stderr);goto done;
 }
 cap=2048;form=calloc(cap,1);
 if(!form)goto done;
 if(!append_form(curl,form,cap,&used,"csrf_token",token)||
    !append_form(curl,form,cap,&used,"username",user)||
    !append_form(curl,form,cap,&used,"password",pass))goto done;
 memset(pass,0,sizeof pass);
 if(!request(curl,S2_SITE "/auth/login",form,page)){
  fputs("[S2] website login rejected\n",stderr);goto done;
 }
 memset(form,0,cap);free(form);form=NULL;
 if(!request(curl,S2_SITE "/admin/page?edit=sai-dsa",NULL,page)){
  fputs("[S2] sai-dsa editor unavailable or not authorized\n",stderr);goto done;
 }
 if(!edit_page(page,token,sizeof token,title,sizeof title,
               description,sizeof description,previous,limit)){
  fprintf(stderr,"[S2] sai-dsa editing form invalid (bytes=%zu)\n",page->length);
  goto done;
 }
 cap=3U*strlen(report)+3U*strlen(description)+3U*strlen(title)+4096U;
 form=calloc(cap,1);if(!form)goto done;
 used=0;
 if(!append_form(curl,form,cap,&used,"csrf_token",token)||
    !append_form(curl,form,cap,&used,"action","update")||
    !append_form(curl,form,cap,&used,"original_slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"slug","sai-dsa")||
    !append_form(curl,form,cap,&used,"title",title)||
    !append_form(curl,form,cap,&used,"description",description)||
    !append_form(curl,form,cap,&used,"status","published")||
    !append_form(curl,form,cap,&used,"content_file","main.md")||
    !append_form(curl,form,cap,&used,"content",report)){
  fputs("[S2] sai-dsa form encoding failed\n",stderr);goto done;
 }
 if(!request(curl,S2_SITE "/admin/page",form,page)){
  fputs("[S2] sai-dsa update submission rejected\n",stderr);goto done;
 }
 memset(form,0,cap);free(form);form=NULL;
 if(!request(curl,S2_SITE "/admin/page?edit=sai-dsa",NULL,page)){
  fputs("[S2] sai-dsa read-back failed\n",stderr);goto done;
 }
 {
  char new_title[256],new_desc[4096],new_token[256];
  if(!edit_page(page,new_token,sizeof new_token,new_title,sizeof new_title,
                new_desc,sizeof new_desc,verified,limit)||
     strcmp(verified,report)||strcmp(new_title,title)||
     strcmp(new_desc,description)){
   fputs("[S2] sai-dsa read-back differs from submitted content; not verified\n",stderr);
   goto done;
  }
 }
 fputs("[S2] sai-dsa content saved and verified by exact read-back\n",stderr);
 ok=1;
done:
 memset(pass,0,sizeof pass);
 if(form){memset(form,0,cap);free(form);}
 if(curl)curl_easy_cleanup(curl);
 free(page);free(previous);free(verified);
 return ok;
}
