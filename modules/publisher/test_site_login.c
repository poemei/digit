/* Digit publishing login acceptance test. No publishing requests. */
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
 page_t *b=v;size_t n=sz*count;if(sz&&n/sz!=count||n>=sizeof b->data-b->len)return 0;
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
  if(!eq)continue;*eq++=0;v=eq;n=strcspn(v,"\r\n");v[n]=0;
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
static int test_site(const char *host,const char *user,const char *pass){
 CURL *curl=curl_easy_init();page_t *page=calloc(1,sizeof *page);
 char url[256],token[129],*u=NULL,*p=NULL,*t=NULL,*form=NULL;
 long code=0;int result=0;
 if(!curl||!page)goto done;
 curl_easy_setopt(curl,CURLOPT_COOKIEFILE,"");
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,15L);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 snprintf(url,sizeof url,"%s/login",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200)goto done;
 char *start=strstr(page->data,"name=\"csrf_token\" value=\"");
 if(!start)goto done;
 start+=strlen("name=\"csrf_token\" value=\"");
 char *end=strchr(start,'"');
 if(!end||(size_t)(end-start)>=sizeof token||end==start)goto done;
 memcpy(token,start,(size_t)(end-start));token[end-start]=0;
 u=curl_easy_escape(curl,user,0);p=curl_easy_escape(curl,pass,0);t=curl_easy_escape(curl,token,0);
 if(!u||!p||!t)goto done;
 size_t need=strlen(u)+strlen(p)+strlen(t)+64;
 form=malloc(need);if(!form)goto done;
 snprintf(form,need,"username=%s&password=%s&csrf_token=%s",u,p,t);
 snprintf(url,sizeof url,"%s/auth/login",host);
 if(request(curl,url,page,form,&code)!=CURLE_OK||code!=302)goto done;
 snprintf(url,sizeof url,"%s/admin",host);
 if(request(curl,url,page,NULL,&code)!=CURLE_OK||code!=200)goto done;
 if(strstr(page->data,"Management Suite")&&strstr(page->data,"/admin/")){
  result=1;
 }
done:
 printf("%s: %s\n",host,result?"AUTHENTICATED":"NOT VERIFIED");
 if(form){memset(form,0,strlen(form));free(form);}
 if(u)curl_free(u);if(p)curl_free(p);if(t)curl_free(t);
 memset(token,0,sizeof token);
 if(page){memset(page,0,sizeof *page);free(page);}
 if(curl)curl_easy_cleanup(curl);
 return result;
}
int main(void){
 char username[256]={0},password[512]={0};int ok=0;
 if(!get_config(username,sizeof username,password,sizeof password)){
  fprintf(stderr,"Credentials missing or insecure: %s (0600, owner root/Digit)\n",AUTH);return 2;
 }
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return 2;
 ok+=test_site("https://chaos-mvc.org",username,password);
 ok+=test_site("https://www.stn-labz.com",username,password);
 ok+=test_site("https://poemei.com",username,password);
 memset(username,0,sizeof username);memset(password,0,sizeof password);
 curl_global_cleanup();
 printf("Login verification: %d/3 domains\n",ok);
 return ok==3?0:1;
}
