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
static int fix_post(CURL *curl,page_t *page,const char *title,const char *body,
 const char *slug,const char *marker){
 char url[512],token[129]={0},*t=NULL,*te=NULL,*be=NULL,*form=NULL;
 long code=0;int good=0;
 snprintf(url,sizeof url,"https://www.stn-labz.com/admin/posts");
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200)goto done;
 const char *row=strstr(page->data,title);
 if(!row){fprintf(stderr,"Existing post not found: %s\n",title);goto done;}
 const char *link=strstr(row,"/admin/posts/edit/");
 if(!link){fprintf(stderr,"Edit link not found: %s\n",title);goto done;}
 link+=strlen("/admin/posts/edit/");
 char *after=NULL;long id=strtol(link,&after,10);
 if(after==link||id<=0||id>2147483647){fprintf(stderr,"Invalid existing Post ID\n");goto done;}
 snprintf(url,sizeof url,"https://www.stn-labz.com/admin/posts/edit/%ld",id);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200||
    !strstr(page->data,"name=\"featured_image_id\"")||
    !strstr(page->data,"digit.png")||
    !extract_csrf(page,token,sizeof token)){
  fprintf(stderr,"Existing edit form unavailable for Post %ld\n",id);goto done;
 }
 /* Form ID 1 is the user's corrected STN-LABZ media record. */
 t=curl_easy_escape(curl,token,0);
 te=curl_easy_escape(curl,title,0);
 be=curl_easy_escape(curl,body,0);
 if(!t||!te||!be)goto done;
 size_t need=strlen(t)+strlen(te)+strlen(be)+160;
 form=malloc(need);if(!form)goto done;
 snprintf(form,need,"id=%ld&title=%s&body=%s&featured_image_id=1&published=on&csrf_token=%s",
  id,te,be,t);
 snprintf(url,sizeof url,"https://www.stn-labz.com/admin/posts");
 if(request(curl,url,page,form,&code)!=CURLE_OK||code!=302){
  fprintf(stderr,"Edit save failed: HTTP %ld\n",code);goto done;
 }
 snprintf(url,sizeof url,"https://www.stn-labz.com/posts/%s",slug);
 if(request(curl,url,page,NULL,&code)==CURLE_OK&&code==200&&
    strstr(page->data,title)&&strstr(page->data,marker)&&
    strstr(page->data,"class=\"featured-image\"")&&
    strstr(page->data,"/uploads/")){
  good=1;
 }
 fprintf(stderr,"STN-LABZ existing post ID %ld: HTTP=%ld featured_image=%s\n",
  id,code,good?"VERIFIED":"NOT_VERIFIED");
done:
 if(form){memset(form,0,strlen(form));free(form);}
 if(t)curl_free(t);if(te)curl_free(te);if(be)curl_free(be);
 return good;
}
static int repair_site(const char *user,const char *pass){
 CURL *curl=curl_easy_init();page_t *page=calloc(1,sizeof *page);
 char url[256],token[129]={0},*u=NULL,*p=NULL,*t=NULL,*form=NULL;
 long code=0;int started=0,passed=0,logout=0;
 if(!curl||!page)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,20L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 if(request(curl,"https://www.stn-labz.com/login",page,NULL,&code)!=CURLE_OK||
    code!=200||!extract_csrf(page,token,sizeof token))goto done;
 u=curl_easy_escape(curl,user,0);p=curl_easy_escape(curl,pass,0);
 t=curl_easy_escape(curl,token,0);if(!u||!p||!t)goto done;
 size_t need=strlen(u)+strlen(p)+strlen(t)+64;
 form=malloc(need);if(!form)goto done;
 snprintf(form,need,"username=%s&password=%s&csrf_token=%s",u,p,t);
 if(request(curl,"https://www.stn-labz.com/auth/login",page,form,&code)!=CURLE_OK||
    code!=302)goto done;
 started=1;memset(form,0,strlen(form));free(form);form=NULL;
 passed+=fix_post(curl,page,
  "Digit Publisher - Core Posts Integration Verification",
  "STN-LABZ is validating publication through the existing ChAoS MVC **Core Posts** interface.\n\n"
  "Digit is exercising authenticated submission, site-specific Media selection, "
  "publication state, public presentation, and session termination.\n\n"
  "This verification does not represent autonomous editorial operation or a production announcement.\n\n"
  "Technical verification marker: STNLABZ-DIGIT-CORE-POSTS-20261022\n",
  "digit-publisher---core-posts-integration-verification",
  "STNLABZ-DIGIT-CORE-POSTS-20261022");
 passed+=fix_post(curl,page,
  "Digit Publisher - Direct Image Path Verification",
  "![Digit](/uploads/digit.png)\n\n"
  "Digit is testing ChAoS MVC Core Posts using a direct image path.\n\n"
  "The image is referenced at \`/uploads/digit.png\` without a Media ID lookup.\n\n"
  "Verification marker: STNLABZ-DIGIT-DIRECT-IMAGE-20261022\n",
  "digit-publisher---direct-image-path-verification",
  "STNLABZ-DIGIT-DIRECT-IMAGE-20261022");
 if(request(curl,"https://www.stn-labz.com/admin",page,NULL,&code)!=CURLE_OK||
    code!=200||!extract_csrf(page,token,sizeof token))goto done;
 if(t){curl_free(t);t=NULL;}t=curl_easy_escape(curl,token,0);if(!t)goto done;
 need=strlen(t)+16;form=malloc(need);if(!form)goto done;
 snprintf(form,need,"csrf_token=%s",t);
 if(request(curl,"https://www.stn-labz.com/logout",page,form,&code)!=CURLE_OK||code!=302)goto done;
 if(request(curl,"https://www.stn-labz.com/admin",page,NULL,&code)!=CURLE_OK)goto done;
 logout=code==302||code==401||code==403||(code==200&&!strstr(page->data,"Management Suite"));
done:
 printf("STN-LABZ existing Posts: %d/2 image verified; logout=%s\n",passed,logout?"VERIFIED":started?"NOT_VERIFIED":"NOT_STARTED");
 if(form){memset(form,0,strlen(form));free(form);}
 if(u)curl_free(u);if(p)curl_free(p);if(t)curl_free(t);
 if(page)free(page);if(curl)curl_easy_cleanup(curl);
 return passed==2&&logout;
}
int main(void){
 char username[256]={0},password[512]={0};int ok;
 if(!get_config(username,sizeof username,password,sizeof password))return 2;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return 2;
 ok=repair_site(username,password);
 memset(username,0,sizeof username);memset(password,0,sizeof password);
 curl_global_cleanup();return ok?0:1;
}
