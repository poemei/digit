/* Digit conversation: read-only knowledge retrieval via ABI 1.5.
 * No initial command vocabulary, Core mutation, direct SQLite access,
 * inferred record schema, or synthetic DSD observations.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "../sqlite3/corpus_search.h"

#define CONVERSATION_SERVICE "digit.conversation"
#define WORD_CAP 48
#define QUERY_TERMS_MAX 4

typedef struct {
 char sender[128],account[128],target[128],message[512];
} message_t;
typedef struct {size_t begin,end;} word_t;

static const stnlabz_module_host_t *host;
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
static int search_phrase(const char *text,const word_t *words,
                         size_t start,size_t count,digit_corpus_query_t *query){
 size_t i,n=0;
 if(!count)return 0;
 memset(query,0,sizeof *query);
 for(i=start;i<start+count;i++){
  size_t len=words[i].end-words[i].begin;
  if(n+len+2>=sizeof query->query)return 0;
  if(i>start)query->query[n++]=' ';
  memcpy(query->query+n,text+words[i].begin,len);n+=len;
 }
 query->query[n]=0;return 1;
}
static stnlabz_module_result_t reply(char *buffer,size_t cap,size_t *used,const char *text){
 size_t n=strlen(text);
 if(n>=cap)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(buffer,text,n+1);*used=n;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t answer(const message_t *m,char *response,
                                      size_t capacity,size_t *used,
                                      const stnlabz_module_host_t *api){
 word_t words[WORD_CAP];
 size_t count,width,start,returned;
 stnlabz_module_result_t rc;
 digit_corpus_query_t query;
 digit_corpus_result_t results;
 if(used)*used=0;
 if(!message_valid(m)||!response||!capacity||!used||!api||
    !api->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 count=tokens(m->message,words,WORD_CAP);
 if(!count)return reply(response,capacity,used,"I couldn't identify searchable terms.");
 /* Search literal phrases, longest first, without a fixed command vocabulary.
  * Query terms remain data bound by the SQLite3 service, never raw SQL. */
 for(width=count<QUERY_TERMS_MAX?count:QUERY_TERMS_MAX;width>0;width--){
  for(start=count-width+1;start>0;){
   start--;
   if(!search_phrase(m->message,words,start,width,&query))continue;
   memset(&results,0,sizeof results);
   returned=0;
   rc=api->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&query,sizeof query,
                          &results,sizeof results,&returned);
   if(rc!=STNLABZ_MODULE_OK)return rc;
   if(returned!=sizeof results||results.count>DIGIT_CORPUS_RESULTS_MAX)
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   if(results.count){
    char line[512];int n;
    const digit_corpus_hit_t *hit=&results.hits[0];
    n=snprintf(line,sizeof line,"Corpus #%lld (%s): %s",
      (long long)hit->id,hit->verification_status[0]?
      hit->verification_status:"status unknown",hit->excerpt);
    if(n<0)return STNLABZ_MODULE_ERR_INVALID_STATE;
    line[sizeof line-1]=0;
    return reply(response,capacity,used,line);
   }
  }
 }
 return reply(response,capacity,used,"No matching corpus evidence was found.");
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
 char response[512];word_t w[WORD_CAP];digit_corpus_query_t query;
 message_t m={0};size_t used=0,n;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 check(q,message_valid(&m),0);
 memset(m.message,'x',sizeof m.message);check(q,!message_valid(&m),1);
 memset(&m,0,sizeof m);
 check(q,tokens("explain STN-Labz",w,WORD_CAP)==2,0);
 check(q,search_phrase("explain STN-Labz",w,1,1,&query)&&!strcmp(query.query,"STN-Labz"),0);
 check(q,search_phrase("explain STN-Labz",w,0,2,&query)&&!strcmp(query.query,"explain STN-Labz"),0);
 check(q,tokens("  ",w,WORD_CAP)==0,0);
 check(q,tokens("epimestic",w,WORD_CAP)==1,0);
 check(q,search_phrase("epimestic",w,0,1,&query)&&!strcmp(query.query,"epimestic"),0);
 check(q,!search_phrase("epimestic",w,0,0,&query),1);
 check(q,tokens("one two three",w,WORD_CAP)==3,0);
 check(q,search_phrase("one two three",w,0,3,&query)&&!strcmp(query.query,"one two three"),0);
 check(q,search_phrase("one two three",w,1,2,&query)&&!strcmp(query.query,"two three"),0);
 check(q,search_phrase("one two three",w,2,1,&query)&&!strcmp(query.query,"three"),0);
 check(q,reply(response,sizeof response,&used,"hello")==STNLABZ_MODULE_OK,0);
 check(q,used==5&&!strcmp(response,"hello"),0);
 check(q,reply(response,3,&used,"hello")==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,answer(NULL,response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,answer(&m,response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,service(NULL,0,response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,tokens("STN-Labz",w,WORD_CAP)==1,0);
 check(q,search_phrase("STN-Labz",w,0,1,&query)&&!strcmp(query.query,"STN-Labz"),0);
 check(q,tokens("alpha beta",w,WORD_CAP)==2,0);
 check(q,search_phrase("alpha beta",w,0,2,&query)&&!strcmp(query.query,"alpha beta"),0);
 n=tokens("1",w,WORD_CAP);check(q,n==1,0);
 check(q,q->tests_executed>=20,0);
 return q->tests_executed>=20&&!q->tests_failed&&q->tests_passed==q->tests_executed&&
 q->negative_test_executed&&q->negative_test_passed?
 STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(host||!api||!api->invoke_service||!api->register_service||
    !api->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
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
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="conversation",.name="Conversation knowledge retrieval",
 .version_major=0,.version_minor=3,.version_patch=0,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
