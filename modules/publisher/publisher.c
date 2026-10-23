/* Digit publisher v0.4.2 - ABI 1.5. Evidence-gated publishing.
 * Site authentication contract is configured explicitly, never inferred.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "module.h"
#define CONFIG "/opt/digit/auth/sites/sites.json"
#define SERVICE "digit.publisher.publish"
#define MAX_BODY 16384
typedef struct {
 char site[32],kind[32],category[64],title[256],markdown[MAX_BODY];
 /* Editorial evidence is supplied by Digit's trusted caller, not inferred here. */
 char subject[32];       /* chaos_mvc | stnlabz | poemei_quote */
 char source_actor[128]; /* canonical identity from Digit's identity gate */
 char evidence_ref[512]; /* caller-verified source reference */
 char post_title[256];   /* required for announcements */
 char post_markdown[MAX_BODY];
} publish_request_t;
typedef struct {
 int published;char publication_id[128],url[1024],error[160];
} publish_response_t;

typedef struct {char name[32],base[128],kinds[6][32];int kind_count;} destination_t;
static destination_t destinations[3];
static char username[256],password[512],poemei_account[128];
static const stnlabz_module_host_t *host_api;
static int credentials_ready;
static void erase(void *p,size_t n){volatile unsigned char *b=p;while(n--)*b++=0;}
static int bounded(const char *s,size_t max){return s&&memchr(s,0,max)!=NULL;}
static int secure_config(const char *path){
 struct stat st;
 return path&&lstat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
  (st.st_uid==0||st.st_uid==geteuid())&&!(st.st_mode&077);
}
static int copy(char *dst,size_t cap,const char *src){
 size_t n;if(!dst||!src||(n=strlen(src))>=cap)return 0;
 memcpy(dst,src,n+1);return 1;
}
static const char *json_string(struct json_object *o,const char *key){
 struct json_object *v=NULL;
 if(!o||!json_object_object_get_ex(o,key,&v)||!json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
static int kind_allowed(const char *kind){
 return kind&&(!strcmp(kind,"announcements")||!strcmp(kind,"news")||
  !strcmp(kind,"page")||!strcmp(kind,"page_update")||!strcmp(kind,"posts")||
  !strcmp(kind,"intelligence"));
}
static int editorial_valid(const publish_request_t *p){
 if(!bounded(p->subject,sizeof p->subject)||
    !bounded(p->source_actor,sizeof p->source_actor)||
    !bounded(p->evidence_ref,sizeof p->evidence_ref)||
    !bounded(p->post_title,sizeof p->post_title)||
    !bounded(p->post_markdown,sizeof p->post_markdown)||
    !p->evidence_ref[0])return 0;
 /* Never classify content by searching keywords; the trusted selection
  * pipeline must supply provenance and an explicit, matching subject. */
 if(!strcmp(p->kind,"page_update") &&
    (strcmp(p->site,"stnlabz")||strcmp(p->source_actor,"digit.s2")))return 0;
 if(!strcmp(p->site,"chaos")){
  if(strcmp(p->subject,"chaos_mvc"))return 0;
 }else if(!strcmp(p->site,"stnlabz")){
  if(strcmp(p->subject,"stnlabz"))return 0;
 }else if(!strcmp(p->site,"poemei")){
  /* Caller must supply the account tag from authenticated IRC transport.
   * Compare it against the same existing account Digit already recognizes.
   * Not a substitute for trusted origin/provenance in the calling pipeline. */
  if(strcmp(p->subject,"poemei_quote")||!poemei_account[0]||
     strcasecmp(p->source_actor,poemei_account))return 0;
 }else return 0;
 if(!strcmp(p->kind,"announcements")){
  if(!p->post_title[0]||!p->post_markdown[0])return 0;
 }else if(p->post_title[0]||p->post_markdown[0])return 0;
 return 1;
}
static int input_valid(const publish_request_t *p){
 if(!p||!bounded(p->site,sizeof p->site)||!bounded(p->kind,sizeof p->kind)||
  !bounded(p->category,sizeof p->category)||!bounded(p->title,sizeof p->title)||
  !bounded(p->markdown,sizeof p->markdown)||!p->title[0]||!p->markdown[0]||
  !kind_allowed(p->kind))return 0;
 for(const char *c=p->title;*c;c++)if((unsigned char)*c<32)return 0;
 return 1;
}
static int load_credentials(void){
 const char *path="/opt/digit/auth/sites/login.conf";
 if(!secure_config(path))return 0;
 FILE *f=fopen(path,"r");if(!f)return 0;
 char line[1024];int u=0,p=0;
 while(fgets(line,sizeof line,f)){
  char *eq=strchr(line,'=');if(!eq)continue;
  *eq++=0;eq[strcspn(eq,"\r\n")]=0;
  if(!strcmp(line,"username"))u=copy(username,sizeof username,eq);
  if(!strcmp(line,"password"))p=copy(password,sizeof password,eq);
 }
 erase(line,sizeof line);fclose(f);
 return u&&p;
}
static int load_poemei_identity(void){
 /* Same authoritative IRC config already consumed by Digit's IRC module. */
 const char *path="/opt/digit/state/auth/irc.env";
 if(!secure_config(path))return 0;
 FILE *f=fopen(path,"r");if(!f)return 0;
 char line[1024];int found=0;
 while(fgets(line,sizeof line,f)){
  char *p=line,*sep,*end;
  while(isspace((unsigned char)*p))p++;
  if(!strncmp(p,"export ",7))p+=7;
  if(*p=='#'||!strchr(p,'='))continue;
  sep=strchr(p,'=');*sep++=0;
  end=p+strlen(p);while(end>p&&isspace((unsigned char)end[-1]))*--end=0;
  end=sep+strlen(sep);while(end>sep&&isspace((unsigned char)end[-1]))*--end=0;
  if(strcmp(p,"DIGIT_IRC_MOMMY_ACCOUNT"))continue;
  size_t n=strlen(sep);
  if(n>=2&&((*sep=='"'&&sep[n-1]=='"')||
             (*sep=='\''&&sep[n-1]=='\''))){sep++;n-=2;}
  if(n&&n<sizeof poemei_account&&!memchr(sep,'\r',n)&&!memchr(sep,'\n',n)){
   memcpy(poemei_account,sep,n);poemei_account[n]=0;found=1;
  }
 }
 erase(line,sizeof line);fclose(f);
 return found;
}
static int load_config(void){
 static const char *names[]={"chaos","stnlabz","poemei"};
 static const char *bases[]={"https://chaos-mvc.org","https://www.stn-labz.com","https://poemei.com"};
 if(!secure_config(CONFIG))return 0;
 FILE *f=fopen(CONFIG,"r");if(!f)return 0;
 char data[8192]={0};size_t n=fread(data,1,sizeof data-1,f);int ok=0;
 fclose(f);data[n]=0;
 struct json_object *root=json_tokener_parse(data),*sites=NULL;
 if(!root||!json_object_object_get_ex(root,"sites",&sites)||
    !json_object_is_type(sites,json_type_array))goto done;
 destination_t local[3]={0};int seen[3]={0};
 for(int i=0;i<3;i++){
  copy(local[i].name,sizeof local[i].name,names[i]);
  copy(local[i].base,sizeof local[i].base,bases[i]);
 }
 int count=json_object_array_length(sites);
 if(count>3)goto done;
 for(int i=0;i<count;i++){
  struct json_object *entry=json_object_array_get_idx(sites,i),*enabled=NULL,*kinds=NULL;
  const char *name=json_string(entry,"name");int idx=-1;
  if(!name||!json_object_object_get_ex(entry,"enabled",&enabled)||
     !json_object_is_type(enabled,json_type_boolean))goto done;
  for(int j=0;j<3;j++)if(!strcmp(name,names[j]))idx=j;
  if(idx<0||seen[idx])goto done;seen[idx]=1;
  if(!json_object_get_boolean(enabled))continue;
  if(!json_object_object_get_ex(entry,"kinds",&kinds)||
     !json_object_is_type(kinds,json_type_array))goto done;
  int kc=json_object_array_length(kinds);
  if(kc<1||kc>6)goto done;
  for(int j=0;j<kc;j++){
   struct json_object *v=json_object_array_get_idx(kinds,j);
   if(!json_object_is_type(v,json_type_string))goto done;
   const char *k=json_object_get_string(v);
   if(!kind_allowed(k)||!copy(local[idx].kinds[j],sizeof local[idx].kinds[j],k))goto done;
  }
  local[idx].kind_count=kc;
 }
 memcpy(destinations,local,sizeof local);
 /* The IRC identity is optional to Publisher startup. PoeMei publishing
  * remains fail-closed unless the known account can be read. */
 ok=load_credentials();
 if(ok)(void)load_poemei_identity();
done:
 if(root)json_object_put(root);erase(data,sizeof data);
 if(!ok){erase(username,sizeof username);erase(password,sizeof password);erase(poemei_account,sizeof poemei_account);}
 return ok;
}
typedef struct {char data[262144];size_t len;} page_t;
static size_t collect(char *p,size_t sz,size_t count,void *ctx){
 page_t *b=ctx;if(sz&&count>SIZE_MAX/sz)return 0;
 size_t n=sz*count;if(n>=sizeof b->data-b->len)return 0;
 memcpy(b->data+b->len,p,n);b->len+=n;b->data[b->len]=0;return n;
}
static int http(CURL *c,const char *url,page_t *page,const char *post,long *code){
 page->len=0;page->data[0]=0;
 curl_easy_setopt(c,CURLOPT_URL,url);
 curl_easy_setopt(c,CURLOPT_POSTFIELDS,post);
 curl_easy_setopt(c,CURLOPT_HTTPGET,post?0L:1L);
 curl_easy_setopt(c,CURLOPT_WRITEDATA,page);
 CURLcode rc=curl_easy_perform(c);
 if(rc!=CURLE_OK)return 0;
 curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,code);return 1;
}
static int csrf(const char *html,char *out,size_t cap){
 const char *p=strstr(html,"name=\"csrf_token\" value=\"");
 if(!p)return 0;p+=strlen("name=\"csrf_token\" value=\"");
 const char *e=strchr(p,'"');if(!e||e==p||(size_t)(e-p)>=cap)return 0;
 memcpy(out,p,(size_t)(e-p));out[e-p]=0;return 1;
}
static int append(char *dst,size_t cap,size_t *len,const char *key,CURL *curl,const char *value){
 char *encoded=curl_easy_escape(curl,value,0);if(!encoded)return 0;
 int n=snprintf(dst+*len,cap-*len,"%s%s=%s",*len?"&":"",key,encoded);
 curl_free(encoded);if(n<0||(size_t)n>=cap-*len)return 0;*len+=(size_t)n;return 1;
}
static int slugify(const char *title,char *out,size_t cap){
 size_t n=0;for(const unsigned char *p=(const unsigned char*)title;*p;p++){
  unsigned char c=*p;
  if((c>='a'&&c<='z')||(c>='0'&&c<='9')){if(n+1>=cap)return 0;out[n++]=c;}
  else if(c>='A'&&c<='Z'){if(n+1>=cap)return 0;out[n++]=(char)(c+32);}
  else if(c==' '||c=='-'||c=='_'){if(n&&out[n-1]!='-'){if(n+1>=cap)return 0;out[n++]='-';}}
 }
 while(n&&out[n-1]=='-')n--;
 if(!n)return 0;out[n]=0;return 1;
}
/* Core Posts retains runs of hyphens after replacing runs of non-alphanumerics. */
static int posts_slug(const char *title,char *out,size_t cap){
 size_t n=0;
 for(const unsigned char *p=(const unsigned char*)title;*p;){
  if((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'){
   if(n+1>=cap)return 0;out[n++]=(char)tolower(*p++);
  }else{
   while(*p&&!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))p++;
   if(n+1>=cap)return 0;out[n++]='-';
  }
 }
 while(n&&out[n-1]=='-')n--;if(!n)return 0;out[n]=0;return 1;
}
/* Decode common HTML entities in Core edit-form metadata. Any unknown
 * entity fails closed rather than silently rewriting the page metadata. */
static int html_value(const char *html,const char *name,char *out,size_t cap){
 char needle[96];snprintf(needle,sizeof needle,"name=\"%s\"",name);
 const char *p=strstr(html,needle),*v,*end;size_t n=0;
 if(!p)return 0;
 if(!strcmp(name,"description")){
  v=strchr(p,'>');if(!v)return 0;v++;end=strstr(v,"</textarea>");
 }else{
  v=strstr(p,"value=\"");if(!v)return 0;v+=7;end=strchr(v,'"');
 }
 if(!end)return 0;
 while(v<end){
  char c=*v++;
  if(c=='&'){
   const char *e=memchr(v,';',(size_t)(end-v));if(!e)return 0;
   size_t len=(size_t)(e-v);
   if(len==3&&!memcmp(v,"amp",3))c='&';
   else if(len==4&&!memcmp(v,"quot",4))c='"';
   else if(len==3&&!memcmp(v,"#39",3))c='\'';
   else if(len==2&&!memcmp(v,"lt",2))c='<';
   else if(len==2&&!memcmp(v,"gt",2))c='>';
   else return 0;
   v=e+1;
  }
  if(n+1>=cap)return 0;out[n++]=c;
 }
 out[n]=0;return 1;
}
static int s2_target_edit_form(const char *html){
 return html&&strstr(html,"name=\"original_slug\"")&&strstr(html,"value=\"sai-dsa\"");
}
static int s2_create_form(const char *html){
 return html&&strstr(html,"name=\"action\"")&&strstr(html,"name=\"slug\"")&&
  strstr(html,"name=\"title\"")&&strstr(html,"name=\"content_file\"")&&
  strstr(html,"name=\"status\"")&&strstr(html,"name=\"content\"");
}
static int selected_option(const char *html,const char *value){
 char needle[96];snprintf(needle,sizeof needle,"value=\"%s\"",value);
 const char *p=strstr(html,needle);if(!p)return 0;
 const char *end=strchr(p,'>');if(!end)return 0;
 const char *selected=strstr(p,"selected");
 return selected&&selected<end;
}
/* Extract S2's Executive Summary as plain-text search description.
 * Keep the report itself in the Core Page content field. */
static int s2_executive_description(const char *markdown,char *out,size_t cap){
 const char *start,*end;
 size_t n=0;int spaced=0;
 if(!markdown||!out||cap<2)return 0;
 start=strstr(markdown,"*1. EXECUTIVE SUMMARY*");
 if(!start)return 0;
 start=strstr(start,"\n\n");
 if(!start)return 0;
 start+=2;
 end=strstr(start,"━━━━━━━━");
 if(!end)return 0;
 while(start<end){
  unsigned char c=(unsigned char)*start++;
  if(c=='\n'||c=='\r'||c=='\t'||c==' ') {
   if(n)spaced=1;
   continue;
  }
  if(c=='*'||c=='#'||c=='`')continue;
  if(c<32)return 0;
  if(spaced&&n){if(n+1>=cap)break;out[n++]=' ';spaced=0;}
  if(n+1>=cap)break;
  out[n++]=(char)c;
 }
 while(n&&out[n-1]==' ')n--;
 out[n]=0;
 return n>0;
}
static int image_id(const char *html){
 const char *select=strstr(html,"name=\"featured_image_id\"");
 if(!select)return -1;
 const char *limit=strstr(select,"</select>");
 if(!limit)return -1;
 for(const char *p=select;(p=strstr(p,"<option"))&&p<limit;){
  const char *gt=strchr(p,'>'),*end=strstr(p,"</option>");
  if(!gt||!end||gt>=end||end>limit)break;
  const char *first=gt+1,*last=end;
  while(first<last&&isspace((unsigned char)*first))first++;
  while(last>first&&isspace((unsigned char)last[-1]))last--;
  if((size_t)(last-first)==9&&!memcmp(first,"digit.png",9)){
   const char *v=strstr(p,"value=\"");
   if(!v||v>=gt)return -1;
   v+=7;char *q=NULL;long id=strtol(v,&q,10);
   return q>v&&q<gt&&*q=='"'&&id>=0&&id<=2147483647?(int)id:-1;
  }
  p=end+9;
 }
 return -1;
}
static stnlabz_module_result_t publish(const void *req,size_t bytes,
 void *res,size_t cap,size_t *used,void *ctx){
 const publish_request_t *p=req;publish_response_t answer={0};
 (void)ctx;if(used)*used=0;
 if(!req||bytes!=sizeof *p||!res||cap<sizeof answer||!used||!input_valid(p))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 destination_t *site=NULL;
 for(int i=0;i<3;i++)if(!strcmp(destinations[i].name,p->site))site=&destinations[i];
 if(!credentials_ready||!site||!editorial_valid(p))return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
 int permitted=0;
 for(int k=0;k<site->kind_count;k++){
  if(!strcmp(site->kinds[k],p->kind))permitted=1;
  /* A verified Digit S2 caller updating the existing STN-LABZ Core
   * Page uses the already-authorized Core Page publication capability.
   * No private configuration migration is necessary. */
  if(!strcmp(p->kind,"page_update")&&!strcmp(p->site,"stnlabz")&&
     !strcmp(p->source_actor,"digit.s2")&&
     !strcmp(site->kinds[k],"page"))permitted=1;
 }
 if(!permitted)return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
 /* Ensure an Announcement never goes live without its required Post.
  * The Post is created and verified first; failure prevents Announcement.
  * A previously created Post is not retried automatically. */
 if(!strcmp(p->kind,"announcements")){
  publish_request_t *follow=calloc(1,sizeof *follow);
  publish_response_t follow_answer={0};size_t follow_used=0;
  if(!follow)return STNLABZ_MODULE_ERR_INVALID_STATE;
  copy(follow->site,sizeof follow->site,p->site);
  copy(follow->kind,sizeof follow->kind,"posts");
  copy(follow->subject,sizeof follow->subject,p->subject);
  copy(follow->source_actor,sizeof follow->source_actor,p->source_actor);
  copy(follow->evidence_ref,sizeof follow->evidence_ref,p->evidence_ref);
  copy(follow->title,sizeof follow->title,p->post_title);
  copy(follow->markdown,sizeof follow->markdown,p->post_markdown);
  stnlabz_module_result_t pr=publish(follow,sizeof *follow,&follow_answer,
   sizeof follow_answer,&follow_used,NULL);
  erase(follow,sizeof *follow);free(follow);
  if(pr!=STNLABZ_MODULE_OK||!follow_answer.published){
   copy(answer.error,sizeof answer.error,
    "Required explanatory Post was not published; Announcement withheld.");
   memcpy(res,&answer,sizeof answer);*used=sizeof answer;
   return STNLABZ_MODULE_ERR_INVALID_STATE;
  }
 }
 CURL *c=curl_easy_init();page_t *page=calloc(1,sizeof *page);
 char url[512],form[65536]={0},token[128]={0},slug[256]={0},*html=NULL;
 long code=0;size_t flen=0;int session=0,verified=0,logout=0,create_s2_page=0;
 if(!c||!page)goto done;
 curl_easy_setopt(c,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(c,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
 snprintf(url,sizeof url,"%s/login",site->base);
 if(!http(c,url,page,NULL,&code)||code!=200||!csrf(page->data,token,sizeof token)){
  copy(answer.error,sizeof answer.error,"Login page or CSRF form unavailable.");goto done;
 }
 if(!append(form,sizeof form,&flen,"username",c,username)||
    !append(form,sizeof form,&flen,"password",c,password)||
    !append(form,sizeof form,&flen,"csrf_token",c,token))goto done;
 snprintf(url,sizeof url,"%s/auth/login",site->base);
 if(!http(c,url,page,form,&code)||code!=302){
  copy(answer.error,sizeof answer.error,"ChAoS login rejected or did not redirect.");goto done;
 }
 session=1;erase(form,sizeof form);flen=0;
 const char *admin=!strcmp(p->kind,"announcements")?"/admin/announcements":
  !strcmp(p->kind,"news")?"/admin/news":
  (!strcmp(p->kind,"page")||!strcmp(p->kind,"page_update"))?"/admin/page":
  !strcmp(p->kind,"intelligence")?"/admin/intelligence":"/admin/posts";
 snprintf(url,sizeof url,"%s%s",site->base,admin);
 if(!strcmp(p->kind,"page_update")){
  /* Explicitly locked to the existing S2 public page. Never create it. */
  if(strcmp(p->title,"sai-dsa")){copy(answer.error,sizeof answer.error,"Wrong page target.");goto cleanup;}
  char edit_url[512];
  snprintf(edit_url,sizeof edit_url,"%s/admin/page?edit=sai-dsa",site->base);
  if(!http(c,edit_url,page,NULL,&code)||
     (code!=200&&code!=302&&code!=404)){
   copy(answer.error,sizeof answer.error,"S2 page lookup failed; creation withheld.");goto cleanup;
  }
  if(code==200&&s2_target_edit_form(page->data)){
   if(!csrf(page->data,token,sizeof token)){
    copy(answer.error,sizeof answer.error,"Existing S2 page CSRF missing; creation withheld.");goto cleanup;
   }
  }else{
   /* Reuse the existing authenticated Core Pages creation interface.
    * Never create a duplicate or proceed without a verified create form. */
   snprintf(url,sizeof url,"%s/admin/page",site->base);
   if(!http(c,url,page,NULL,&code)||code!=200||
      !s2_create_form(page->data)||!csrf(page->data,token,sizeof token)){
    copy(answer.error,sizeof answer.error,"S2 page missing; Core Pages create form unavailable.");goto cleanup;
   }
   if(strstr(page->data,"?edit=sai-dsa")){
    copy(answer.error,sizeof answer.error,"S2 page present in listing; duplicate creation refused.");goto cleanup;
   }
   create_s2_page=1;
  }
 }else if(!strcmp(p->kind,"intelligence")){
  char create_url[512];
  snprintf(create_url,sizeof create_url,"%s/admin/intelligence?tab=intelligence&action=add",site->base);
  if(!http(c,create_url,page,NULL,&code)||code!=200||!csrf(page->data,token,sizeof token))goto cleanup;
 }else if(!http(c,url,page,NULL,&code)||code!=200||!csrf(page->data,token,sizeof token))goto cleanup;
 int img=-1;
 if(!strcmp(p->kind,"posts")){img=image_id(page->data);if(img<0)goto cleanup;}
 if(!strcmp(p->kind,"page_update"))snprintf(slug,sizeof slug,"sai-dsa");
 else if(!slugify(p->title,slug,sizeof slug))goto cleanup;
 if(!strcmp(p->kind,"posts")&&!posts_slug(p->title,slug,sizeof slug))goto cleanup;
 /* Fail closed: publishing an identical title twice is never deliberate. */
 if(strcmp(p->kind,"page_update")&&(strstr(page->data,p->title)||strstr(page->data,slug))){
  copy(answer.error,sizeof answer.error,"Existing title or slug; publication refused.");
  goto cleanup;
 }
 if(!strcmp(p->kind,"announcements")){
  if(!strstr(page->data,"name=\"action\" value=\"create\""))goto cleanup;
 }else if(!strstr(page->data,"name=\"title\""))goto cleanup;
 erase(form,sizeof form);flen=0;
 if(!strcmp(p->kind,"page_update")){
  char original_title[256]={0},description[1024]={0};
  if(!create_s2_page){
   if(!html_value(page->data,"title",original_title,sizeof original_title)||
      !html_value(page->data,"description",description,sizeof description)||
      !strstr(page->data,"name=\"content_file\"")||
      !selected_option(page->data,"published")){
    copy(answer.error,sizeof answer.error,"Existing page metadata/status check failed.");goto cleanup;
   }
  }else copy(original_title,sizeof original_title,p->title);
  if(!s2_executive_description(p->markdown,description,sizeof description)){
   copy(answer.error,sizeof answer.error,"S2 Executive Summary missing; no page description generated.");
   goto cleanup;
  }
  /* Core Pages can update an existing main.html/main.txt page to main.md.
   * The S2 source is Markdown; a previous content-file format must not
   * silently block updates to the existing Core Page. */
  if(!append(form,sizeof form,&flen,"csrf_token",c,token)||
     !append(form,sizeof form,&flen,"action",c,create_s2_page?"create":"update")||
     (!create_s2_page&&!append(form,sizeof form,&flen,"original_slug",c,"sai-dsa"))||
     !append(form,sizeof form,&flen,"slug",c,"sai-dsa")||
     !append(form,sizeof form,&flen,"title",c,original_title)||
     !append(form,sizeof form,&flen,"description",c,description)||
     !append(form,sizeof form,&flen,"status",c,"published")||
     !append(form,sizeof form,&flen,"content_file",c,"main.md")||
     !append(form,sizeof form,&flen,"content",c,p->markdown)){
   copy(answer.error,sizeof answer.error,"Core Page create/update form encoding failed.");goto cleanup;
  }
 }else if(!append(form,sizeof form,&flen,"csrf_token",c,token)||
    !append(form,sizeof form,&flen,"title",c,p->title))goto cleanup;
 if(!strcmp(p->kind,"page_update")){
  /* Exact update form has already been constructed. */
 }else if(!strcmp(p->kind,"announcements")){
  if(!append(form,sizeof form,&flen,"action",c,"create")||
     !append(form,sizeof form,&flen,"body",c,p->markdown)||
     !append(form,sizeof form,&flen,"published",c,"1"))goto cleanup;
 }else if(!strcmp(p->kind,"posts")){
  char id[32];snprintf(id,sizeof id,"%d",img);
  if(!append(form,sizeof form,&flen,"body",c,p->markdown)||
     !append(form,sizeof form,&flen,"featured_image_id",c,id)||
     !append(form,sizeof form,&flen,"published",c,"on"))goto cleanup;
 }else if(!strcmp(p->kind,"intelligence")){
  if(!append(form,sizeof form,&flen,"action",c,"create_intelligence")||
     !append(form,sizeof form,&flen,"content",c,p->markdown)||
     !append(form,sizeof form,&flen,"source",c,p->evidence_ref)||
     !append(form,sizeof form,&flen,"status",c,"ACTIVE"))goto cleanup;
 }else if(!strcmp(p->kind,"page")){
  if(!append(form,sizeof form,&flen,"action",c,"create")||
     !append(form,sizeof form,&flen,"slug",c,slug)||
     !append(form,sizeof form,&flen,"description",c,"Digit publication")||
     !append(form,sizeof form,&flen,"status",c,"published")||
     !append(form,sizeof form,&flen,"content_file",c,"main.md")||
     !append(form,sizeof form,&flen,"content",c,p->markdown))goto cleanup;
 }else{
  if(!append(form,sizeof form,&flen,"action",c,"create")||
     !append(form,sizeof form,&flen,"slug",c,slug)||
     !append(form,sizeof form,&flen,"category",c,p->category[0]?p->category:"development")||
     !append(form,sizeof form,&flen,"summary",c,"Digit News publication")||
     !append(form,sizeof form,&flen,"body_markdown",c,p->markdown)||
     !append(form,sizeof form,&flen,"status",c,"published"))goto cleanup;
 }
 if(!http(c,url,page,form,&code)||code!=302){
  if(!strcmp(p->kind,"page_update"))
   copy(answer.error,sizeof answer.error,"Core Page create/update POST failed or lacked redirect.");
  goto cleanup;
 }
 erase(form,sizeof form);flen=0;
 if(!strcmp(p->kind,"page_update")){
  char public_url[512];
  const char *marker=strstr(p->markdown,"S2-PUBLIC-SNAPSHOT-");
  char expected[96]={0};
  if(!marker)goto cleanup;
  size_t marker_length=strcspn(marker," \r\n\t");
  if(marker_length<22||marker_length>=sizeof expected)goto cleanup;
  memcpy(expected,marker,marker_length);
  snprintf(public_url,sizeof public_url,"%s/sai-dsa",site->base);
  if(http(c,public_url,page,NULL,&code)&&code==200&&
     strstr(page->data,expected)!=NULL){
   verified=1;copy(answer.url,sizeof answer.url,public_url);
   copy(answer.publication_id,sizeof answer.publication_id,"sai-dsa");
  }else copy(answer.error,sizeof answer.error,"Public sai-dsa page did not contain submitted snapshot.");
 }else if(!strcmp(p->kind,"announcements")){
  if(http(c,url,page,NULL,&code)&&code==200&&strstr(page->data,p->title))verified=1;
  if(verified)snprintf(answer.url,sizeof answer.url,"%s/announcements",site->base);
 }else if(!strcmp(p->kind,"intelligence")){
  /* The module assigns its own INT identifier, not a title-based slug.
   * Verify the new entry in the authenticated admin listing and public index. */
  if(http(c,url,page,NULL,&code)&&code==200&&strstr(page->data,p->title)){
   char public_url[512];
   snprintf(public_url,sizeof public_url,"%s/intelligence",site->base);
   if(http(c,public_url,page,NULL,&code)&&code==200&&strstr(page->data,p->title)){
    verified=1;
    copy(answer.url,sizeof answer.url,public_url);
    copy(answer.publication_id,sizeof answer.publication_id,"verified-index");
   }
  }
 }else{
  const char *route=!strcmp(p->kind,"posts")?"posts/":
   !strcmp(p->kind,"news")?"news/":"";
  snprintf(url,sizeof url,"%s/%s%s",site->base,route,slug);
  if(http(c,url,page,NULL,&code)&&code==200&&strstr(page->data,p->title)){
   if(!strcmp(p->kind,"posts")){
    verified=strstr(page->data,"/uploads/")!=NULL;
   }else verified=1;
  }
  if(verified)copy(answer.url,sizeof answer.url,url);
 }
 if(verified){answer.published=1;
  if(strcmp(p->kind,"intelligence"))copy(answer.publication_id,sizeof answer.publication_id,slug);
 }
cleanup:
 /* Logout is required even when publishing or verification fails. */
 if(session){
  snprintf(url,sizeof url,"%s/admin",site->base);
  if(http(c,url,page,NULL,&code)&&code==200&&csrf(page->data,token,sizeof token)){
   erase(form,sizeof form);flen=0;
   if(append(form,sizeof form,&flen,"csrf_token",c,token)){
    snprintf(url,sizeof url,"%s/logout",site->base);
    if(http(c,url,page,form,&code)&&code==302){
     snprintf(url,sizeof url,"%s/admin",site->base);
     if(http(c,url,page,NULL,&code)&&
        (code==302||code==401||code==403||(code==200&&!strstr(page->data,"Management Suite"))))
      logout=1;
    }
   }
  }
 }
done:
 if(!logout){
  answer.published=0;
  if(!answer.error[0])copy(answer.error,sizeof answer.error,"Logout/session verification failed.");
 }
 if(!answer.published&&!answer.error[0])
  copy(answer.error,sizeof answer.error,"Submission, public verification, or logout failed.");
 if(res&&cap>=sizeof answer&&used){memcpy(res,&answer,sizeof answer);*used=sizeof answer;}
 erase(form,sizeof form);erase(token,sizeof token);
 if(page)free(page);if(c)curl_easy_cleanup(c);
 return answer.published?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static void check(stnlabz_module_qualification_result_t *r,int pass,int negative){
 r->tests_executed++;if(pass)r->tests_passed++;else r->tests_failed++;
 if(negative){r->negative_test_executed=1;if(pass)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r){
 publish_request_t p={0};char out[32]={0},slug[256]={0};
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 const char *valid[]={"announcements","news","page","posts","intelligence"};
 for(int i=0;i<5;i++)check(r,kind_allowed(valid[i]),0);
 check(r,kind_allowed("intelligence"),0);
 check(r,kind_allowed("page_update"),0);
 {publish_request_t gate={0};
  copy(gate.site,sizeof gate.site,"poemei");
  copy(gate.kind,sizeof gate.kind,"page_update");
  copy(gate.subject,sizeof gate.subject,"poemei_quote");
  copy(gate.evidence_ref,sizeof gate.evidence_ref,"source");
  check(r,!editorial_valid(&gate),1);
  copy(gate.site,sizeof gate.site,"stnlabz");
  copy(gate.subject,sizeof gate.subject,"stnlabz");
  copy(gate.source_actor,sizeof gate.source_actor,"digit.s2");
  check(r,editorial_valid(&gate),0);
 }
 check(r,!kind_allowed("srt"),1);
 check(r,!kind_allowed("admin"),1);
 check(r,!kind_allowed(NULL),1);
 check(r,copy(out,sizeof out,"hello"),0);
 check(r,!copy(out,3,"hello"),1);
 check(r,!copy(NULL,sizeof out,"x"),1);
 check(r,!copy(out,sizeof out,NULL),1);
 check(r,slugify("Digit News - Test",slug,sizeof slug)&&!strcmp(slug,"digit-news-test"),0);
 check(r,posts_slug("Digit News - Test",slug,sizeof slug)&&!strcmp(slug,"digit-news---test"),0);
 check(r,!slugify("",slug,sizeof slug),1);
 check(r,!posts_slug("",slug,sizeof slug),1);
 check(r,!secure_config("/definitely/missing/publisher.conf"),1);
 check(r,image_id("<select name=\"featured_image_id\"><option value=\"1\"> digit.png </option></select>")==1,0);
 check(r,image_id("<select name=\"featured_image_id\"><option value=\"0\"> digit.png </option></select>")==0,0);
 check(r,image_id("<select name=\"featured_image_id\"><option value=\"\"> No Image </option></select>")==-1,1);
 {
  /* Full-size unclassified S2 body may expand threefold when URL-encoded. */
  CURL *test_curl=curl_easy_init();
  char *large_body=malloc(MAX_BODY);
  char *encoded_form=calloc(1,65536);
  size_t encoded_len=0;
  int ok=test_curl&&large_body&&encoded_form;
  if(ok){
   memset(large_body,'&',MAX_BODY-1);large_body[MAX_BODY-1]=0;
   ok=append(encoded_form,65536,&encoded_len,"content",test_curl,large_body)
      &&encoded_len>MAX_BODY && encoded_len<65536;
  }
  check(r,ok,0);
  if(test_curl)curl_easy_cleanup(test_curl);
  free(large_body);free(encoded_form);
 }
 {
  char desc[1024]={0};
  const char *report="Snapshot: S2-PUBLIC-SNAPSHOT-10\n\n*1. EXECUTIVE SUMMARY*\n\nAPI severity HIGH under investigation.\n\nRictus: accessible | Sentinel: accessible\n\n━━━━━━━━\n\n*2. ACTIVE INVESTIGATIONS*";
  check(r,s2_executive_description(report,desc,sizeof desc)&&strstr(desc,"Rictus: accessible")!=NULL,0);
  check(r,!s2_executive_description("No executive section",desc,sizeof desc),1);
  {
   const char *edit="<input name=\"original_slug\" value=\"sai-dsa\">";
   const char *create="<input name=\"action\"><input name=\"slug\"><input name=\"title\"><input name=\"content_file\"><input name=\"status\"><textarea name=\"content\"></textarea>";
   check(r,s2_target_edit_form(edit)!=0,0);
   check(r,!s2_target_edit_form(create),1);
   check(r,s2_create_form(create)!=0,0);
   check(r,!s2_create_form("<input name=\"title\">"),1);
   check(r,!s2_target_edit_form("<input name=\"original_slug\" value=\"other\">"),1);
  }
 }
 check(r,!csrf("<input name=\"csrf_token\" value=\"\">",out,sizeof out),1);
 check(r,csrf("<input name=\"csrf_token\" value=\"abc\">",out,sizeof out)&&!strcmp(out,"abc"),0);
 copy(p.site,sizeof p.site,"chaos");copy(p.kind,sizeof p.kind,"news");
 copy(p.title,sizeof p.title,"Digit");copy(p.markdown,sizeof p.markdown,"Content");
 copy(p.subject,sizeof p.subject,"chaos_mvc");
 copy(p.evidence_ref,sizeof p.evidence_ref,"verified-source");
 check(r,input_valid(&p),0);
 check(r,editorial_valid(&p),0);
 copy(p.site,sizeof p.site,"poemei");check(r,!editorial_valid(&p),1);
 copy(poemei_account,sizeof poemei_account,"TestPoeAccount");
 copy(p.subject,sizeof p.subject,"poemei_quote");
 copy(p.source_actor,sizeof p.source_actor,"TestPoeAccount");
 check(r,editorial_valid(&p),0);
 copy(p.source_actor,sizeof p.source_actor,"UnverifiedNick");
 check(r,!editorial_valid(&p),1);
 erase(poemei_account,sizeof poemei_account);
 copy(p.subject,sizeof p.subject,"chaos_mvc");
 copy(p.site,sizeof p.site,"chaos");
 copy(p.subject,sizeof p.subject,"stnlabz");check(r,!editorial_valid(&p),1);
 copy(p.subject,sizeof p.subject,"chaos_mvc");
 copy(p.kind,sizeof p.kind,"announcements");check(r,!editorial_valid(&p),1);
 copy(p.post_title,sizeof p.post_title,"Details");
 copy(p.post_markdown,sizeof p.post_markdown,"Evidence");check(r,editorial_valid(&p),0);
 copy(p.kind,sizeof p.kind,"news");check(r,!editorial_valid(&p),1);
 p.post_title[0]=0;p.post_markdown[0]=0;
 p.markdown[0]=0;check(r,!input_valid(&p),1);
 copy(p.markdown,sizeof p.markdown,"Content");
 copy(p.kind,sizeof p.kind,"srt");check(r,!input_valid(&p),1);
 return r->tests_failed||r->tests_executed<STNLABZ_MODULE_MIN_TESTS||
  !r->negative_test_executed||!r->negative_test_passed?
  STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 if(host_api||!host||!host->register_service||!host->unregister_service||
   !host->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Configuration affects permission to publish, not module activation.
  * Missing or malformed config fails closed in publish(), while still
  * allowing the module to activate and expose a service. */
 credentials_ready=load_config();
 if(!credentials_ready)
  fprintf(stderr,"publisher: publication disabled; site configuration or credentials unavailable\n");
 if(!host->register_service(SERVICE,publish,NULL)){
  fprintf(stderr,"publisher: service registration failed for %s\n",SERVICE);
  erase(destinations,sizeof destinations);erase(username,sizeof username);erase(password,sizeof password);erase(poemei_account,sizeof poemei_account);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host_api=host;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host_api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host_api->unregister_service(SERVICE,NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host_api=NULL;credentials_ready=0;erase(destinations,sizeof destinations);erase(username,sizeof username);erase(password,sizeof password);erase(poemei_account,sizeof poemei_account);return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="publisher",.name="Digit Markdown Publisher",
 .version_major=0,.version_minor=4,.version_patch=10,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
