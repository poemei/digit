/* Digit Observation: append-only IRC channel evidence, ABI 1.5.
 * Operator owns /opt/digit/observation; this module never modifies SQLite.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "module.h"
#include "observation.h"

#define OBSERVATION_PATH "/opt/digit/observation/observation.jsonl"
static const stnlabz_module_host_t *api;
static int journal=-1;
static pthread_mutex_t journal_lock=PTHREAD_MUTEX_INITIALIZER;

static int valid(const char *value,size_t cap,int mandatory){
 size_t i;
 if(!value||!memchr(value,0,cap))return 0;
 for(i=0;value[i];i++){
  unsigned char c=(unsigned char)value[i];
  if(c<32||c==127)return 0;
 }
 return !mandatory||i>0;
}
static int record_valid(const digit_observation_record_t *r){
 return r&&
  valid(r->transport,sizeof r->transport,1)&&
  !strcmp(r->transport,"irc")&&
  valid(r->channel,sizeof r->channel,1)&&
  !strcmp(r->channel,"#stn-labz")&&
  valid(r->sender,sizeof r->sender,1)&&
  valid(r->account,sizeof r->account,0)&&
  valid(r->text,sizeof r->text,1);
}
static int append_json(char *out,size_t cap,size_t *used,const char *s){
 const unsigned char *p=(const unsigned char *)s;
 size_t n=*used;
 if(n+2>cap)return 0;
 out[n++]='"';
 while(*p){
  unsigned char c=*p++;
  if(c=='"'||c=='\\'){
   if(n+2>=cap)return 0;
   out[n++]='\\';out[n++]=(char)c;
  }else if(c<32){
   if(n+6>=cap)return 0;
   {static const char hex[]="0123456789abcdef";
    out[n++]='\\';out[n++]='u';out[n++]='0';out[n++]='0';
    out[n++]=hex[c>>4];out[n++]=hex[c&15];}
  }else{
   if(n+1>=cap)return 0;
   out[n++]=(char)c;
  }
 }
 if(n+1>=cap)return 0;
 out[n++]='"';*used=n;return 1;
}
static int build_line(const digit_observation_record_t *r,char *line,size_t cap,size_t *used){
 struct timespec now;
 struct tm utc;
 char when[32];
 size_t n=0;
 if(!record_valid(r)||clock_gettime(CLOCK_REALTIME,&now)!=0||
    !gmtime_r(&now.tv_sec,&utc)||
    !strftime(when,sizeof when,"%Y-%m-%dT%H:%M:%SZ",&utc))return 0;
 #define LIT(x) do{const char *v=(x);size_t z=strlen(v);if(n+z>=cap)return 0;memcpy(line+n,v,z);n+=z;}while(0)
 LIT("{\"timestamp\":");
 if(!append_json(line,cap,&n,when))return 0;
 LIT(",\"transport\":");
 if(!append_json(line,cap,&n,r->transport))return 0;
 LIT(",\"channel\":");
 if(!append_json(line,cap,&n,r->channel))return 0;
 LIT(",\"sender\":");
 if(!append_json(line,cap,&n,r->sender))return 0;
 LIT(",\"account\":");
 if(!append_json(line,cap,&n,r->account))return 0;
 LIT(",\"text\":");
 if(!append_json(line,cap,&n,r->text))return 0;
 LIT("}\n");
 #undef LIT
 *used=n;return 1;
}
static stnlabz_module_result_t record_service(
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *response_used,void *context){
 char line[2048];size_t length=0,off=0;
 const digit_observation_record_t *r=request;
 (void)response;(void)response_size;(void)context;
 if(response_used)*response_used=0;
 if(request_size!=sizeof *r||!build_line(r,line,sizeof line,&length))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 pthread_mutex_lock(&journal_lock);
 if(journal<0){pthread_mutex_unlock(&journal_lock);return STNLABZ_MODULE_ERR_INVALID_STATE;}
 while(off<length){
  ssize_t n=write(journal,line+off,length-off);
  if(n<0&&errno==EINTR)continue;
  if(n<=0)break;
  off+=(size_t)n;
 }
 pthread_mutex_unlock(&journal_lock);
 return off==length?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static void check(stnlabz_module_qualification_result_t *q,int yes,int negative){
 q->tests_executed++;
 if(yes)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(yes)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_observation_record_t r={0};
 char line[2048];size_t used=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 strcpy(r.transport,"irc");strcpy(r.channel,"#stn-labz");
 strcpy(r.sender,"Rictus");strcpy(r.account,"rictus");
 strcpy(r.text,"[S2] \"intel\" \\ sample");
 check(q,record_valid(&r),0);
 check(q,build_line(&r,line,sizeof line,&used),0);
 check(q,used>0&&line[used-1]=='\n',0);
 check(q,strstr(line,"\\\"intel\\\"")!=NULL,0);
 check(q,strstr(line,"\\\\ sample")!=NULL,0);
 check(q,!record_valid(NULL),1);
 strcpy(r.transport,"slack");check(q,!record_valid(&r),1);
 strcpy(r.transport,"irc");
 strcpy(r.channel,"#other");check(q,!record_valid(&r),1);
 strcpy(r.channel,"#stn-labz");
 strcpy(r.text,"bad\nmessage");check(q,!record_valid(&r),1);
 strcpy(r.text,"message");check(q,record_valid(&r),0);
 memset(r.account,'x',sizeof r.account);check(q,!record_valid(&r),1);
 r.account[0]=0;check(q,record_valid(&r),0);
 check(q,record_service(NULL,0,NULL,0,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,record_service(&r,sizeof r,NULL,0,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_STATE,1);
 return !q->tests_failed?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 struct stat st;
 int fd;
 if(api||!host||!host->register_service||!host->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Operator provisions directory. Never follow a symlink or truncate a log. */
 fd=open(OBSERVATION_PATH,O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd<0)return STNLABZ_MODULE_ERR_START_FAILED;
 if(fstat(fd,&st)!=0||!S_ISREG(st.st_mode)){close(fd);return STNLABZ_MODULE_ERR_START_FAILED;}
 journal=fd;api=host;
 if(!host->register_service(DIGIT_OBSERVATION_RECORD_SERVICE,record_service,NULL)){
  journal=-1;api=NULL;close(fd);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!api)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!api->unregister_service(DIGIT_OBSERVATION_RECORD_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 pthread_mutex_lock(&journal_lock);
 if(journal>=0)close(journal);
 journal=-1;api=NULL;
 pthread_mutex_unlock(&journal_lock);
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="observation",.name="IRC observation journal",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
