/* Digit Core Posts publishing acceptance: chaos-mvc.org and stn-labz.com. */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define AUTH "/opt/digit/auth/sites/login.conf"
typedef struct {char data[262144];size_t len;} page_t;
static size_t collect(char *p,size_t sz,size_t count,void *v){
 page_t *b=v;size_t n=sz*count;if((sz&&n/sz!=count)||n>=sizeof b->data-b->len)return 0;
 memcpy(b->data+b->len,p,n);b->len+=n;b->data[b->len]=0;return n;
}
static int get_config(char *user,size_t uc,char *pass,size_t pc){
 struct stat st;FILE *f;char line[1024];int u=0,p=0;
 if(lstat(AUTH,&st)){perror("credentials file stat");return 0;}
 if(!S_ISREG(st.st_mode)||st.st_nlink!=1){
  fprintf(stderr,"Credentials must be a regular, unlinked-once file.\n");return 0;
 }
 if(st.st_uid!=geteuid()&&st.st_uid!=0){
  fprintf(stderr,"Credential file owner must be the test account or root.\n");return 0;
 }
 if(st.st_mode&077){
  fprintf(stderr,"Credential file permissions expose group/other access; require 0600.\n");return 0;
 }
 f=fopen(AUTH,"r");if(!f){perror("credentials file open");return 0;}
 while(fgets(line,sizeof line,f)){
  char *eq=strchr(line,'='),*v;size_t n;
  if(!eq)continue;
  *eq++=0;
  v=eq;
  n=strcspn(v,"\r\n");
  v[n]=0;
  if(!strcmp(line,"username")&&n>0&&n<uc){memcpy(user,v,n+1);u=1;}
  if(!strcmp(line,"password")&&n>0&&n<pc){memcpy(pass,v,n+1);p=1;}
 }
 fclose(f);memset(line,0,sizeof line);return u&&p;
}
static CURLcode request(CURL *curl,const char *url,page_t *page,const char *form,long *status){
 page->len=0;page->data[0]=0;
 curl_easy_setopt(curl,CURLOPT_URL,url);
 curl_easy_setopt(curl,CURLOPT_POSTFIELDS,form);
 curl_easy_setopt(curl,CURLOPT_HTTPGET,form?0L:1L);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,page);
 CURLcode rc=curl_easy_perform(curl);
 if(rc==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,status);
 return rc;
}
static int extract_csrf(const page_t *page,char *token,size_t cap){
 const char *start=strstr(page->data,"name=\"csrf_token\" value=\"");
 if(!start)return 0;
 start+=strlen("name=\"csrf_token\" value=\"");
 const char *end=strchr(start,'"');
 if(!end||end==start||(size_t)(end-start)>=cap)return 0;
 memcpy(token,start,(size_t)(end-start));token[end-start]=0;
 return 1;
}

/* Verify the published page, not merely the admin listing. */
static int verify_public_post(CURL *curl,page_t *page,const char *host,const char *title,const char *slug,const char *marker){
 char url[256];
 long code=0;
 snprintf(url,sizeof url,"%s/posts/%s",host,slug);
 CURLcode rc=request(curl,url,page,NULL,&code);
 int title_ok=rc==CURLE_OK&&strstr(page->data,title)!=NULL;
 int body_ok=rc==CURLE_OK&&strstr(page->data,marker)!=NULL;
 int image_ok=rc==CURLE_OK&&strstr(page->data,"/uploads/")!=NULL;
 fprintf(stderr,"%s: public verification HTTP=%ld title=%s body=%s image=%s\n",
  host,code,title_ok?"YES":"NO",body_ok?"YES":"NO",image_ok?"YES":"NO");
 return rc==CURLE_OK&&code==200&&title_ok&&body_ok&&image_ok;
}
/* Verified against stnlabz/chaos-mvc Core posts controller and view. */
static int post_site(const char *host,const char *user,const char *pass,
 const char *title,const char *body,const char *slug,const char *marker){
 CURL *curl=curl_easy_init();
 page_t *page=calloc(1,sizeof *page);
 char url[256],token[129]={0},*u=NULL,*p=NULL,*t=NULL,*title_encoded=NULL,*body_encoded=NULL,*form=NULL;
 long code=0;
 int authenticated=0,created=0,found=0,logout_posted=0,logout_verified=0,session_started=0,already_present=0;
 if(!curl||!page)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 snprintf(url,sizeof url,"%s/login",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200||
    !extract_csrf(page,token,sizeof token))goto done;
 u=curl_easy_escape(curl,user,0);p=curl_easy_escape(curl,pass,0);
 t=curl_easy_escape(curl,token,0);
 if(!u||!p||!t)goto done;
 size_t need=strlen(u)+strlen(p)+strlen(t)+64;
 form=malloc(need);if(!form)goto done;
 snprintf(form,need,"username=%s&password=%s&csrf_token=%s",u,p,t);
 snprintf(url,sizeof url,"%s/auth/login",host);
 if(request(curl,url,page,form,&code)!=CURLE_OK||code!=302)goto done;
 session_started=1;
 memset(form,0,strlen(form));free(form);form=NULL;
 snprintf(url,sizeof url,"%s/admin/posts",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200)goto cleanup;
 authenticated=1;
 if(!strstr(page->data,"name=\"title\"")||
    !strstr(page->data,"name=\"published\"")||
    !strstr(page->data,"name=\"body\"")||
    !extract_csrf(page,token,sizeof token)){
  fprintf(stderr,"%s: verified Core Posts create form not found; refusing submission\n",host);
  goto cleanup;
 }
 /* Core renders <option value="ID" ...> whitespace filename whitespace </option>.
  * Parse only this selector, trim both sides of its text, and reject ambiguity. */
 const char *media=strstr(page->data,"name=\"featured_image_id\"");
 if(!media){fprintf(stderr,"%s: Media selector missing\n",host);goto cleanup;}
 const char *select_end=strstr(media,"</select>");
 if(!select_end){fprintf(stderr,"%s: Media selector incomplete\n",host);goto cleanup;}
 int image_id=0,matched=0;
 const char *cursor=media;
 while((cursor=strstr(cursor,"<option"))&&cursor<select_end){
  const char *open_end=strchr(cursor,'>');
  const char *option_end=strstr(cursor,"</option>");
  if(!open_end||!option_end||open_end>=option_end||option_end>select_end)break;
  const char *first=open_end+1,*last=option_end;
  while(first<last&&isspace((unsigned char)*first))first++;
  while(last>first&&isspace((unsigned char)last[-1]))last--;
  if((size_t)(last-first)==sizeof("digit.png")-1&&
     !memcmp(first,"digit.png",sizeof("digit.png")-1)){
   /* Read the value attribute only within this option's opening tag. */
   const char *attr=cursor;
   while(attr<open_end){
    while(attr<open_end&&isspace((unsigned char)*attr))attr++;
    if(attr>=open_end)break;
    if((size_t)(open_end-attr)>=5&&!memcmp(attr,"value",5)){
     const char *v=attr+5;
     while(v<open_end&&isspace((unsigned char)*v))v++;
     if(v<open_end&&*v=='='){
      v++;
      while(v<open_end&&isspace((unsigned char)*v))v++;
      char quote=0;
      if(v<open_end&&(*v=='"'||*v=='\''))quote=*v++;
      char *endptr=NULL;
      long id=strtol(v,&endptr,10);
      if(endptr>v&&endptr<open_end&&id>0&&id<=2147483647&&
         (quote?*endptr==quote:(isspace((unsigned char)*endptr)||*endptr=='>'))){
       image_id=(int)id;matched++;
      }else{
       fprintf(stderr,"%s: digit.png option ID not parseable; option opening tag: [%.*s]\n",
        host,(int)((open_end-cursor)>200?200:(open_end-cursor+1)),cursor);
      }
      break;
     }
    }
    while(attr<open_end&&!isspace((unsigned char)*attr))attr++;
   }

  }
  cursor=option_end+9;
 }
 if(matched!=1){
  fprintf(stderr,"%s: digit.png Media entries found in Core Posts dropdown: %d (expected exactly one)\n",host,matched);
  int shown=0;
  const char *scan=media;
  while((scan=strstr(scan,"<option"))&&scan<select_end&&shown<30){
   const char *gt=strchr(scan,'>'),*end=strstr(scan,"</option>");
   if(!gt||!end||gt>=end||end>select_end)break;
   fprintf(stderr,"%s: dropdown label %d: [%.*s]\\n",host,++shown,
    (int)((end-gt-1)>100?100:(end-gt-1)),gt+1);
   scan=end+9;
  }
  goto cleanup;
 }
 if(strstr(page->data,title)){
  already_present=1;
  printf("%s: post already exists; not submitting duplicate\n",host);
  found=verify_public_post(curl,page,host,title,slug,marker);
  goto cleanup;
 }
 title_encoded=curl_easy_escape(curl,title,0);
 body_encoded=curl_easy_escape(curl,body,0);
 if(t){curl_free(t);t=NULL;}
 t=curl_easy_escape(curl,token,0);
 if(!title_encoded||!body_encoded||!t)goto cleanup;
 need=strlen(title_encoded)+strlen(body_encoded)+strlen(t)+160;
 form=malloc(need);if(!form)goto cleanup;
 snprintf(form,need,"title=%s&body=%s&published=on&featured_image_id=%d&csrf_token=%s",
   title_encoded,body_encoded,image_id,t);
 if(request(curl,url,page,form,&code)!=CURLE_OK||code!=302){
  fprintf(stderr,"%s: post POST not confirmed (HTTP %ld)\n",host,code);
  goto cleanup;
 }
 created=1;
 memset(form,0,strlen(form));free(form);form=NULL;
 if(request(curl,url,page,NULL,&code)==CURLE_OK&&code==200&&strstr(page->data,title))
  found=verify_public_post(curl,page,host,title,slug,marker);
 else fprintf(stderr,"%s: saved Post was not found in the admin listing (HTTP %ld)\n",host,code);

cleanup:
 /* Logout on all paths after a session is established. */
 snprintf(url,sizeof url,"%s/admin",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200||
    !extract_csrf(page,token,sizeof token))goto done;
 if(t){curl_free(t);t=NULL;}
 t=curl_easy_escape(curl,token,0);if(!t)goto done;
 need=strlen(t)+16;form=malloc(need);if(!form)goto done;
 snprintf(form,need,"csrf_token=%s",t);
 snprintf(url,sizeof url,"%s/logout",host);
 logout_posted=request(curl,url,page,form,&code)==CURLE_OK&&code==302;
 if(!logout_posted)goto done;
 snprintf(url,sizeof url,"%s/admin",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK)goto done;
 logout_verified=code==302||code==401||code==403||
  (code==200&&!strstr(page->data,"Management Suite"));
done:
 printf("%s: authenticated=%s post=%s logout=%s\n",host,
  authenticated?"YES":"NO",
  created&&found?"CREATED_AND_LISTED":already_present&&found?"ALREADY_EXISTS":
  created?"POSTED_NOT_VERIFIED":"NOT_PUBLISHED",
  logout_verified?"VERIFIED":session_started?"NOT_VERIFIED":"NOT_STARTED");
 if(form){memset(form,0,strlen(form));free(form);}
 if(u)curl_free(u);
 if(p)curl_free(p);
 if(t)curl_free(t);
 if(title_encoded)curl_free(title_encoded);
 if(body_encoded)curl_free(body_encoded);
 memset(token,0,sizeof token);
 if(page){memset(page,0,sizeof *page);free(page);}
 if(curl)curl_easy_cleanup(curl);
 return authenticated&&(created||already_present)&&found&&logout_verified;
}
int main(void){
 char username[256]={0},password[512]={0};int ok=0;
 if(!get_config(username,sizeof username,password,sizeof password)){
  fprintf(stderr,"Credentials missing or insecure: %s\n",AUTH);return 2;
 }
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return 2;
 ok+=post_site("https://chaos-mvc.org",username,password,
  "Digit Core Posts Integration - Technical Verification",
  "Digit is testing ChAoS MVC **Core Posts** using her own level-7 account.\n\n"
  "This post exercises the existing authenticated form, CSRF validation, Media selection, "
  "published checkbox, public route, and logout.\n\n"
  "This is a controlled integration verification, not proof of autonomous editorial scheduling.\n\n"
  "Technical verification marker: CHAOS-DIGIT-CORE-POSTS-20261022\n",
  "digit-core-posts-integration---technical-verification",
  "CHAOS-DIGIT-CORE-POSTS-20261022");
 ok+=post_site("https://www.stn-labz.com",username,password,
  "Digit Publisher - Core Posts Integration Verification",
  "STN-LABZ is validating publication through the existing ChAoS MVC **Core Posts** interface.\n\n"
  "Digit is exercising authenticated submission, site-specific Media selection, "
  "publication state, public presentation, and session termination.\n\n"
  "This verification does not represent autonomous editorial operation or a production announcement.\n\n"
  "Technical verification marker: STNLABZ-DIGIT-CORE-POSTS-20261022\n",
  "digit-publisher---core-posts-integration-verification",
  "STNLABZ-DIGIT-CORE-POSTS-20261022");
 memset(username,0,sizeof username);memset(password,0,sizeof password);
 curl_global_cleanup();
 printf("Core Posts acceptance: %d/2 domains\n",ok);
 return ok==2?0:1;
}
