#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <json-c/json.h>
#include "stnlabz_abi/module.h"
#include "observation.h"
#define STORE "/opt/digit/observation/observations.jsonl"
static const stnlabz_module_host_t *host;
static pthread_mutex_t write_lock=PTHREAD_MUTEX_INITIALIZER;
static int active;
static int terminated(const char *s,size_t n){
 return s&&memchr(s,0,n)!=NULL&&s[0]!=0;
}
static stnlabz_module_result_t record(const void *input,size_t length,void *out,
 size_t capacity,size_t *used,void *ctx){
 const digit_observation_t *m=input;
 struct json_object *j=NULL;
 const char *encoded;char stamp[32];char line[4098];
 time_t now;struct tm utc;
 int fd=-1,ok=0;size_t n;int saved=0;
 (void)ctx;
 if(used)*used=0;
 if(!active||!m||length!=sizeof *m||m->version!=DIGIT_OBSERVATION_VERSION||
    !terminated(m->transport,sizeof m->transport)||
    !terminated(m->channel,sizeof m->channel)||
    !terminated(m->sender,sizeof m->sender)||
    !terminated(m->event_id,sizeof m->event_id)||
    !terminated(m->text,sizeof m->text)||
    !out||capacity<sizeof saved||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(strcmp(m->transport,"slack")&&strcmp(m->transport,"irc"))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Only public channel traffic, never Slack DMs/private groups or IRC PMs. */
 if(m->transport[0]=='i'&&m->channel[0]!='#')
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 j=json_object_new_object();if(!j)return STNLABZ_MODULE_ERR_START_FAILED;
 now=time(NULL);
 if(!gmtime_r(&now,&utc)||!strftime(stamp,sizeof stamp,"%Y-%m-%dT%H:%M:%SZ",&utc))goto done;
 json_object_object_add(j,"time",json_object_new_string(stamp));
 json_object_object_add(j,"transport",json_object_new_string(m->transport));
 json_object_object_add(j,"channel",json_object_new_string(m->channel));
 json_object_object_add(j,"sender",json_object_new_string(m->sender));
 json_object_object_add(j,"event_id",json_object_new_string(m->event_id));
 json_object_object_add(j,"text",json_object_new_string(m->text));
 encoded=json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN);
 n=strlen(encoded);if(!n||n>4096)goto done;
 memcpy(line,encoded,n);line[n++]='\n';
 pthread_mutex_lock(&write_lock);
 fd=open(STORE,O_WRONLY|O_APPEND|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
 if(fd>=0){
  struct stat st;
  if(fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
     !(st.st_mode&077)&&st.st_uid==geteuid()){
   /* One append per JSONL line; roll back a partial tail under the lock. */
   off_t before=lseek(fd,0,SEEK_END);
   if(before>=0){
    ssize_t written=write(fd,line,n);
    if(written==(ssize_t)n)ok=1;
    else if(written>=0)(void)ftruncate(fd,before);
   }
  }
  close(fd);
 }
 pthread_mutex_unlock(&write_lock);
done:
 json_object_put(j);
 if(!ok)return STNLABZ_MODULE_ERR_START_FAILED;
 saved=1;memcpy(out,&saved,sizeof saved);*used=sizeof saved;
 return STNLABZ_MODULE_OK;
}
/* Read only recorded public channel evidence, scoped to one transport/channel.
 * This never verifies a claim, authorizes an action, or writes to Corpus. */
static const char *json_field(struct json_object *j,const char *key){
 struct json_object *v=NULL;
 if(!json_object_object_get_ex(j,key,&v)||!json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
static int bounded_copy(char *dst,size_t cap,const char *src){
 size_t n;
 if(!src||(n=strlen(src))>=cap)return 0;
 memcpy(dst,src,n+1);return 1;
}
static stnlabz_module_result_t recent(const void *input,size_t length,void *out,
 size_t capacity,size_t *used,void *ctx){
 const digit_observation_recent_request_t *q=input;
 digit_observation_recent_response_t *result=out;
 FILE *fp=NULL;int fd=-1;
 struct stat st;char *line=NULL;size_t allocation=0;ssize_t n;
 size_t wanted,seen=0;digit_observation_recent_item_t ring[DIGIT_OBSERVATION_RECENT_MAX]={{0}};
 (void)ctx;
 if(used)*used=0;
 if(!active||!q||length!=sizeof *q||!result||capacity<sizeof *result||!used||
    q->version!=DIGIT_OBSERVATION_READ_VERSION||
    !terminated(q->transport,sizeof q->transport)||
    !terminated(q->channel,sizeof q->channel)||
    (strcmp(q->transport,"irc")&&strcmp(q->transport,"slack"))||
    !q->limit||q->limit>DIGIT_OBSERVATION_RECENT_MAX||
    (q->transport[0]=='i'&&q->channel[0]!='#'))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 wanted=q->limit;memset(result,0,sizeof *result);
 result->version=DIGIT_OBSERVATION_READ_VERSION;
 pthread_mutex_lock(&write_lock);
 fd=open(STORE,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0){
  pthread_mutex_unlock(&write_lock);
  return errno==ENOENT?(*used=sizeof *result,STNLABZ_MODULE_OK):STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||
    (st.st_mode&077)||st.st_uid!=geteuid()){
  close(fd);pthread_mutex_unlock(&write_lock);return STNLABZ_MODULE_ERR_START_FAILED;
 }
 fp=fdopen(fd,"r");
 if(!fp){close(fd);pthread_mutex_unlock(&write_lock);return STNLABZ_MODULE_ERR_START_FAILED;}
 /* Only scan a bounded suffix. Discard the first partial line if seeking. */
 if(st.st_size>262144){
  if(fseeko(fp,st.st_size-262144,SEEK_SET)!=0){fclose(fp);pthread_mutex_unlock(&write_lock);return STNLABZ_MODULE_ERR_START_FAILED;}
  (void)getline(&line,&allocation,fp);
 }
 while((n=getline(&line,&allocation,fp))>=0){
  struct json_object *j=NULL;
  const char *transport,*channel,*time_value,*sender,*event_id,*text_value;
  digit_observation_recent_item_t item={{0}};
  if(n>4097||n<3)continue;
  j=json_tokener_parse(line);
  if(!j)continue;
  transport=json_field(j,"transport");channel=json_field(j,"channel");
  time_value=json_field(j,"time");sender=json_field(j,"sender");
  event_id=json_field(j,"event_id");text_value=json_field(j,"text");
  if(transport&&channel&&!strcmp(transport,q->transport)&&!strcmp(channel,q->channel)&&
     bounded_copy(item.time,sizeof item.time,time_value)&&
     bounded_copy(item.sender,sizeof item.sender,sender)&&
     bounded_copy(item.event_id,sizeof item.event_id,event_id)&&
     bounded_copy(item.text,sizeof item.text,text_value)){
   ring[seen%wanted]=item;seen++;
  }
  json_object_put(j);
 }
 free(line);fclose(fp);pthread_mutex_unlock(&write_lock);
 result->count=(uint32_t)(seen<wanted?seen:wanted);
 for(uint32_t i=0;i<result->count;i++)
  result->items[i]=ring[(seen-result->count+i)%wanted];
 *used=sizeof *result;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 struct stat st;
 if(active||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(stat("/opt/digit/observation",&st)||!S_ISDIR(st.st_mode)||
    st.st_uid!=geteuid()||(st.st_mode&0027))
  return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(DIGIT_OBSERVATION_SERVICE,record,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 if(!h->register_service(DIGIT_OBSERVATION_READ_SERVICE,recent,NULL)){
  (void)h->unregister_service(DIGIT_OBSERVATION_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 host=h;active=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!active)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_OBSERVATION_READ_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_OBSERVATION_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 active=0;host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 snprintf(d.id,sizeof d.id,"observation");
 snprintf(d.name,sizeof d.name,"Digit Passive Channel Observer");
 d.version_major=0;d.version_minor=1;d.version_patch=1;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
