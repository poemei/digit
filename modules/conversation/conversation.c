/* Digit conversation: authenticated private knowledge retrieval via ABI 1.5.
 * No initial command vocabulary, Core mutation, direct SQLite access,
 * inferred record schema, or synthetic DSD observations.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

#define CONVERSATION_SERVICE "digit.conversation"
#define SQLITE_GET_SERVICE "digit.sqlite3.get"
#define AUTH_FILE "/opt/digit/state/auth/conversation.env"
#define KEY_CAP 128
#define VALUE_CAP 1024
#define WORD_CAP 32

typedef struct {
 char sender[128],account[128],target[128],message[512];
} message_t;
typedef struct {
 char key[KEY_CAP];
 unsigned char value[VALUE_CAP];
 size_t length;
} kv_record_t;
typedef struct {size_t begin,end;} word_t;

static const stnlabz_module_host_t *host;
static char permitted_account[128];

static int safe_account(const char *s,size_t cap){
 size_t i;
 if(!s||!memchr(s,0,cap)||!s[0])return 0;
 for(i=0;s[i];i++){
  unsigned char c=(unsigned char)s[i];
  if(!isalnum(c)&&c!='_'&&c!='-'&&c!='.')return 0;
 }
 return 1;
}
static int auth_read(char *out,size_t cap){
 FILE *f=fopen(AUTH_FILE,"r");
 char line[256];
 size_t n;
 if(!f)return 0;
 if(!fgets(line,sizeof line,f)){fclose(f);return 0;}
 fclose(f);
 n=strcspn(line,"\r\n");
 if(n==sizeof line-1&&line[n])return 0;
 line[n]=0;
 if(!safe_account(line,sizeof line)||strlen(line)>=cap)return 0;
 memcpy(out,line,strlen(line)+1);
 return 1;
}
static int message_valid(const message_t *m){
 return m&&memchr(m->sender,0,sizeof m->sender)&&
  memchr(m->account,0,sizeof m->account)&&
  memchr(m->target,0,sizeof m->target)&&
  memchr(m->message,0,sizeof m->message);
}
static int token_character(unsigned char c){
 return isalnum(c)||c=='_'||c=='-'||c=='.'||c=='/';
}
static size_t tokens(const char *message,word_t *words,size_t cap){
 size_t pos=0,count=0;
 while(message[pos]){
  while(message[pos]&&!token_character((unsigned char)message[pos]))pos++;
  if(!message[pos])break;
  if(count==cap)return 0;
  words[count].begin=pos;
  while(message[pos]&&token_character((unsigned char)message[pos]))pos++;
  words[count++].end=pos;
 }
 return count;
}
static int key_candidate(const char *text,const word_t *words,
                         size_t first,size_t last,char *out){
 size_t i,offset=0;
 if(last<first)return 0;
 for(i=first;i<=last;i++){
  size_t n=words[i].end-words[i].begin;
  if(n==0||offset+n+(i!=first)>=KEY_CAP)return 0;
  if(i!=first)out[offset++]='.';
  memcpy(out+offset,text+words[i].begin,n);
  offset+=n;
 }
 out[offset]=0;
 return 1;
}
static int plain_text(const unsigned char *data,size_t length){
 size_t i;
 if(!length||length>VALUE_CAP)return 0;
 for(i=0;i<length;i++){
  unsigned char c=data[i];
  if(c<32||c==127)return 0;
 }
 return 1;
}
static stnlabz_module_result_t answer(const message_t *m,char *response,
                                      size_t capacity,size_t *used,
                                      const stnlabz_module_host_t *api){
 word_t words[WORD_CAP];
 size_t count,span,start,returned;
 stnlabz_module_result_t rc;
 kv_record_t found;
 char key[KEY_CAP];
 if(used)*used=0;
 if(!message_valid(m)||!response||!capacity||!used||!api||
    !api->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Authorization is explicit, configured locally and never inferred from
  * nicknames or user text. Public-channel lookup is prohibited. */
 if(!permitted_account[0]||strcmp(m->account,permitted_account)||
    strcmp(m->sender,m->target))
  return STNLABZ_MODULE_ERR_NOT_AUTHORIZED;
 count=tokens(m->message,words,WORD_CAP);
 if(!count){
  const char *text="I couldn't identify a knowledge key in that message.";
  size_t n=strlen(text);
  if(n>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(response,text,n+1);*used=n;return STNLABZ_MODULE_OK;
 }
 /* Prefer the longest contiguous literal key. No verb/prefix dictionary.
  * This is retrieval, not general-purpose natural-language understanding. */
 for(span=count;span>0;span--){
  for(start=0;start+span<=count;start++){
   if(!key_candidate(m->message,words,start,start+span-1,key))continue;
   memset(&found,0,sizeof found);
   memset(found.key,0,sizeof found.key);
   memcpy(found.key,key,strlen(key)+1);
   returned=0;
   rc=api->invoke_service(SQLITE_GET_SERVICE,found.key,KEY_CAP,
                          &found,sizeof found,&returned);
   if(rc==STNLABZ_MODULE_ERR_NOT_FOUND)continue;
   if(rc!=STNLABZ_MODULE_OK)return rc;
   if(returned!=sizeof found||found.length>VALUE_CAP)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   if(!plain_text(found.value,found.length)){
    const char *msg="The matching record is not safe to display as text.";
    size_t n=strlen(msg);
    if(n>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memcpy(response,msg,n+1);*used=n;return STNLABZ_MODULE_OK;
   }
   {
    size_t n=found.length;
    if(n>=capacity)n=capacity-1;
    memcpy(response,found.value,n);
    response[n]=0;
    *used=n;
    return STNLABZ_MODULE_OK;
   }
  }
 }
 {
  const char *msg="I couldn't find a matching knowledge record.";
  size_t n=strlen(msg);
  if(n>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  memcpy(response,msg,n+1);*used=n;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t service(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *used,void *context){
 (void)context;
 if(used)*used=0;
 if(!request||request_size!=sizeof(message_t))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 return answer((const message_t *)request,(char *)response,response_size,
               used,host);
}
static void check(stnlabz_module_qualification_result_t *q,int good,int negative){
 q->tests_executed++;
 if(good)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(good)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 char key[KEY_CAP],response[512];
 word_t w[WORD_CAP];
 message_t m={0};
 size_t used=0,n;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 check(q,safe_account("PoeMei",128),0);
 check(q,!safe_account("",128),1);
 check(q,!safe_account("a b",128),1);
 check(q,!safe_account(NULL,128),1);
 check(q,!safe_account("a\nb",128),1);
 check(q,tokens("explain STN-Labz",w,WORD_CAP)==2,0);
 check(q,key_candidate("explain STN-Labz",w,1,1,key)&&!strcmp(key,"STN-Labz"),0);
 check(q,key_candidate("explain STN-Labz",w,0,1,key)&&!strcmp(key,"explain.STN-Labz"),0);
 check(q,tokens("  ",w,WORD_CAP)==0,0);
 check(q,tokens("one two three",w,WORD_CAP)==3,0);
 check(q,key_candidate("one two three",w,0,2,key)&&!strcmp(key,"one.two.three"),0);
 check(q,!key_candidate("one",w,2,1,key),1);
 check(q,plain_text((const unsigned char *)"hello",5),0);
 check(q,!plain_text((const unsigned char *)"",0),1);
 check(q,!plain_text((const unsigned char *)"x\0",2),1);
 check(q,!plain_text((const unsigned char *)"x\r",2),1);
 check(q,!plain_text((const unsigned char *)"x\n",2),1);
 check(q,message_valid(&m),0);
 memset(m.message,'x',sizeof m.message);
 check(q,!message_valid(&m),1);
 memset(m.message,0,sizeof m.message);
 check(q,message_valid(&m),0);
 strcpy(m.sender,"PoeMei");
 strcpy(m.target,"PoeMei");
 strcpy(m.account,"PoeMei");
 strcpy(m.message,"explain STN-Labz");
 check(q,answer(&m,response,sizeof response,&used,NULL)==
         STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 /* Failure to configure an operator account never opens the database. */
 check(q,safe_account("ExampleAccount",128),0);
 check(q,service(NULL,0,response,sizeof response,&used,NULL)==
         STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,!key_candidate("abc",w,2,1,key),1);
 n=tokens("Hello/World",w,WORD_CAP);
 check(q,n==1,0);
 check(q,key_candidate("Hello/World",w,0,0,key)&&
         !strcmp(key,"Hello/World"),0);
 check(q,q->tests_executed>=20,0);
 return q->tests_failed||q->tests_passed!=q->tests_executed?
  STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->invoke_service||!api->register_service||
    !api->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Missing or invalid configuration leaves the service closed. */
 memset(permitted_account,0,sizeof permitted_account);
 (void)auth_read(permitted_account,sizeof permitted_account);
 if(!api->register_service(CONVERSATION_SERVICE,service,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=api;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(CONVERSATION_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;
 memset(permitted_account,0,sizeof permitted_account);
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="conversation",.name="Conversation knowledge retrieval",
 .version_major=0,.version_minor=1,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
