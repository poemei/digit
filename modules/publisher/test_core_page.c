/* Digit Core Pages publishing acceptance test, chaos-mvc.org only. */
#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
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

/* Verified against poemei/announcements v1.3.5 controller and admin form.
 * Only chaos-mvc.org and stn-labz.com: two-site acceptance scope.
 * The form may differ if deployed modules are not the same version.
 */
static int page_site(const char *host,const char *user,const char *pass){
 CURL *curl=curl_easy_init();
 page_t *page=calloc(1,sizeof *page);
 char url[256],token[129]={0},*u=NULL,*p=NULL,*t=NULL,*title_encoded=NULL,*body_encoded=NULL,*form=NULL;
 const char *title="Digit Pages Integration Verification";
 const char *body="Digit is verifying the existing ChAoS MVC **Core Pages** publishing form.\n\n"
  "This page is managed through the filesystem-backed Pages system.\n\n"
  "Verification marker: DIGIT-PAGES-CHAOS-20261022\n";
 const char *slug="digit-pages-integration-verification";
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
 snprintf(url,sizeof url,"%s/admin/page",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200)goto cleanup;
 authenticated=1;
 if(!strstr(page->data,"name=\"action\"")||
    !strstr(page->data,"name=\"slug\"")||
    !strstr(page->data,"name=\"content_file\"")||
    !strstr(page->data,"name=\"status\"")||
    !strstr(page->data,"name=\"content\"")||
    !extract_csrf(page,token,sizeof token)){
  fprintf(stderr,"%s: Core Pages form not found; no submission\n",host);
  goto cleanup;
 }
 if(strstr(page->data,slug)){
  already_present=1;
  printf("%s: page already exists; not submitting duplicate\n",host);
  snprintf(url,sizeof url,"%s/%s",host,slug);
  if(request(curl,url,page,NULL,&code)==CURLE_OK&&code==200&&strstr(page->data,"DIGIT-PAGES-CHAOS-20261022"))found=1;
  goto cleanup;
 }
 title_encoded=curl_easy_escape(curl,title,0);
 body_encoded=curl_easy_escape(curl,body,0);
 if(t){curl_free(t);t=NULL;}
 t=curl_easy_escape(curl,token,0);
 if(!title_encoded||!body_encoded||!t)goto cleanup;
 need=strlen(title_encoded)+strlen(body_encoded)+strlen(t)+256;
 form=malloc(need);if(!form)goto cleanup;
 snprintf(form,need,"action=create&slug=%s&title=%s&description=Digit+Core+Pages+acceptance&status=published&content_file=main.md&content=%s&csrf_token=%s",
   slug,title_encoded,body_encoded,t);
 if(request(curl,url,page,form,&code)!=CURLE_OK||code!=302){
  fprintf(stderr,"%s: page POST not confirmed (HTTP %ld)\n",host,code);
  goto cleanup;
 }
 created=1;
 memset(form,0,strlen(form));free(form);form=NULL;
 if(request(curl,url,page,NULL,&code)==CURLE_OK&&code==200&&strstr(page->data,slug)){
  snprintf(url,sizeof url,"%s/%s",host,slug);
  if(request(curl,url,page,NULL,&code)==CURLE_OK&&code==200&&
     strstr(page->data,"DIGIT-PAGES-CHAOS-20261022"))found=1;
 }
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
 printf("%s: authenticated=%s page=%s logout=%s\n",host,
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
 ok+=page_site("https://chaos-mvc.org",username,password);
 memset(username,0,sizeof username);memset(password,0,sizeof password);
 curl_global_cleanup();
 printf("Core Pages acceptance: %d/1 domain\n",ok);
 return ok==1?0:1;
}
