#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "stnlabz_abi/module.h"
#include "research_contract.h"
#define CONFIG "/opt/digit/auth/research/research.conf"
#define AUDIT "/opt/digit/research/research.log"
static const stnlabz_module_host_t *owner;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int active;
static int private_file(const char *path){
 struct stat st;
 return lstat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
        !(st.st_mode&077)&&(st.st_uid==0||st.st_uid==geteuid());
}
static int valid_dir(const char *path){
 struct stat st;
 return lstat(path,&st)==0&&S_ISDIR(st.st_mode)&&!(st.st_mode&0022)&&
        (st.st_uid==0||st.st_uid==geteuid());
}
int digit_research_mission_allowed(const char *mission){
 size_t i,n;if(!mission)return 0;n=strnlen(mission,DIGIT_RESEARCH_MISSION_MAX);
 if(!n||n>=DIGIT_RESEARCH_MISSION_MAX)return 0;
 for(i=0;i<n;i++)if((unsigned char)mission[i]<32||(unsigned char)mission[i]>126)return 0;
 for(i=0;i<n;i++)if(!isspace((unsigned char)mission[i]))return 1;
 return 0;
}
int digit_research_url_allowed(const char *url){
 const char *host,*end,*p;
 size_t n;
 if(!url||strncmp(url,"https://",8))return 0;
 n=strnlen(url,DIGIT_RESEARCH_URL_MAX);
 if(n<17||n>=DIGIT_RESEARCH_URL_MAX)return 0;
 host=url+8;end=host;
 while(*end&&*end!='/'&&*end!='?'&&*end!='#')end++;
 if(end==host||end-host>253||host[0]=='.'||end[-1]=='.')return 0;
 for(p=host;p<end;p++){
  if(!isalnum((unsigned char)*p)&&*p!='-'&&*p!='.')return 0;
 }
 /* Require a public-looking DNS hostname; reject IP literals and localhost. */
 if(!memchr(host,'.',(size_t)(end-host)))return 0;
 for(p=host;p<end;p++)if(isalpha((unsigned char)*p))break;
 if(p==end)return 0;
 if((size_t)(end-host)>=9&&!strncasecmp(host,"localhost",9)&&
    (host+9==end||host[9]=='.'))return 0;
 if((size_t)(end-host)>=6&&!strncasecmp(host,"local.",6))return 0;
 if((size_t)(end-host)>=9&&!strncasecmp(end-9,".internal",9))return 0;
 if((size_t)(end-host)>=6&&!strncasecmp(end-6,".local",6))return 0;
 for(p=url;p<url+n;p++)
  if((unsigned char)*p<33||(unsigned char)*p>126||*p=='\\')return 0;
 /* URL must not contain embedded credentials or encoded controls. */
 for(p=url;p+2<url+n;p++)
  if(*p=='%'&&((p[1]=='0'&&(p[2]=='a'||p[2]=='A'||p[2]=='d'||p[2]=='D'))||
               (p[1]=='2'&&p[2]=='5')))return 0;
 return 1;
}
static int config_ok(void){
 FILE *fp;char line[128];int enabled=0,valid=1;
 if(!private_file(CONFIG))return 0;
 fp=fopen(CONFIG,"r");if(!fp)return 0;
 while(fgets(line,sizeof line,fp)){
  size_t n=strlen(line);
  if(!n||line[n-1]!='\n'){valid=0;break;}
  line[n-1]=0;
  if(!line[0]||line[0]=='#')continue;
  if(!strcmp(line,"enabled=true"))enabled=1;
  else if(!strcmp(line,"enabled=false"))enabled=0;
  else {valid=0;break;}
 }
 if(ferror(fp))valid=0;
 fclose(fp);return valid&&enabled;
}
static size_t receive(char *ptr,size_t size,size_t count,void *context){
 digit_research_result_t *r=context;size_t n,old;
 if(count&&size>SIZE_MAX/count)return 0;
 n=size*count;
 old=strlen(r->excerpt);
 if(n>=sizeof r->excerpt-old){
  size_t available=sizeof r->excerpt-old-1;
  if(available){memcpy(r->excerpt+old,ptr,available);r->excerpt[old+available]=0;}
  r->source_truncated=1;
  return n;
 }
 memcpy(r->excerpt+old,ptr,n);
 r->excerpt[old+n]=0;
 return n;
}
static int audit(const char *mission,const char *url,int status,const char *outcome){
 int fd,ok=0;FILE *fp;time_t now=time(NULL);
 if(!valid_dir("/opt/digit/research"))return 0;
 fd=open(AUDIT,O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW,0600);
 if(fd<0)return 0;
 fp=fdopen(fd,"a");if(!fp){close(fd);return 0;}
 if(fprintf(fp,"%lld mission=%s url=%s status=%d outcome=%s\n",
            (long long)now,mission,url,status,outcome)>0&&
    fflush(fp)==0&&fsync(fileno(fp))==0)ok=1;
 if(fclose(fp)!=0)ok=0;
 return ok;
}
static stnlabz_module_result_t retrieve(const void *request,size_t length,
 void *response,size_t capacity,size_t *used,void *unused){
 const digit_research_request_t *req=request;
 digit_research_result_t result={0};
 CURL *curl=NULL;CURLcode rc;long http=0;char *type=NULL;
 time_t now=time(NULL);struct tm utc={0};
 int valid,logged;
 (void)unused;
 if(used)*used=0;
 if(!active||!req||length!=sizeof *req||!response||capacity<sizeof result||!used||
    req->version!=DIGIT_RESEARCH_CONTRACT_VERSION||
    !memchr(req->url,0,sizeof req->url)||!memchr(req->mission,0,sizeof req->mission)||
    !digit_research_url_allowed(req->url)||!digit_research_mission_allowed(req->mission))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result.version=DIGIT_RESEARCH_CONTRACT_VERSION;
 snprintf(result.url,sizeof result.url,"%s",req->url);
 if(gmtime_r(&now,&utc))strftime(result.retrieved_at,sizeof result.retrieved_at,
                                "%Y-%m-%dT%H:%M:%SZ",&utc);
 pthread_mutex_lock(&lock);
 curl=curl_easy_init();
 if(!curl){pthread_mutex_unlock(&lock);return STNLABZ_MODULE_ERR_START_FAILED;}
 curl_easy_setopt(curl,CURLOPT_URL,req->url);
 curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_PROXY,"");
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
 curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-Research/0.1.0");
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,6L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,15L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&result);
 rc=curl_easy_perform(curl);
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
 curl_easy_getinfo(curl,CURLINFO_CONTENT_TYPE,&type);
 if(type)snprintf(result.content_type,sizeof result.content_type,"%s",type);
 result.http_status=(int)http;
 valid=rc==CURLE_OK&&http==200;
 logged=audit(req->mission,req->url,(int)http,valid?(result.source_truncated?"RETRIEVED_TRUNCATED":"RETRIEVED"):
                 result.source_truncated?"SIZE_LIMIT":"RETRIEVAL_FAILED");
 curl_easy_cleanup(curl);
 pthread_mutex_unlock(&lock);
 if(!logged&&owner&&owner->send_message)
  (void)owner->send_message("[RESEARCH] Mandatory retrieval audit unavailable");
 if(!logged)return STNLABZ_MODULE_ERR_AUDIT_FULL;
 if(!valid)return STNLABZ_MODULE_ERR_NOT_FOUND;
 memcpy(response,&result,sizeof result);*used=sizeof result;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 int passed=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 passed+=digit_research_url_allowed("https://www.php.net/releases/");
 passed+=digit_research_url_allowed("https://www.example.com/a?q=1");
 passed+=!digit_research_url_allowed("http://www.example.com");
 passed+=!digit_research_url_allowed("https://127.0.0.1/");
 passed+=!digit_research_url_allowed("https://localhost/");
 passed+=!digit_research_url_allowed("https://example.com@internal/");
 passed+=!digit_research_url_allowed("https://[::1]/");
 passed+=!digit_research_url_allowed("https://example.com:8080/");
 passed+=digit_research_mission_allowed("Research PHP releases");
 passed+=!digit_research_mission_allowed(" ");
 q->tests_executed=10;q->tests_passed=(unsigned)passed;q->tests_failed=(unsigned)(10-passed);
 q->negative_test_executed=1;q->negative_test_passed=passed==10;
 return passed==10?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(active||!h||!h->register_service||!h->unregister_service||!h->send_message)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!config_ok()||!valid_dir("/opt/digit/research"))return STNLABZ_MODULE_ERR_START_FAILED;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return STNLABZ_MODULE_ERR_START_FAILED;
 owner=h;active=1;
 if(!h->register_service(DIGIT_RESEARCH_SERVICE,retrieve,NULL)){
  active=0;owner=NULL;curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(owner&&owner->unregister_service)
  (void)owner->unregister_service(DIGIT_RESEARCH_SERVICE,NULL);
 active=0;owner=NULL;curl_global_cleanup();return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static const stnlabz_module_descriptor_t d={
  "research","Digit Research",0,1,1,STNLABZ_MODULE_API_MAJOR,
  STNLABZ_MODULE_API_MINOR,qualify,start,stop};
 return &d;
}
