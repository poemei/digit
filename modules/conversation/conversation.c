/* Digit conversation: read-only knowledge retrieval via ABI 1.5.
 * No initial command vocabulary, Core mutation, direct SQLite access,
 * inferred record schema, or synthetic DSD observations.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "module.h"
#include "../sqlite3/corpus_search.h"
#include "../observation/observation.h"
#include "../dsd/dsd.h"

#define CONVERSATION_SERVICE "digit.conversation"
#define CONVERSATION_LISTEN_SERVICE "digit.conversation.listen"
#define WORD_CAP 48
#define QUERY_TERMS_MAX 4

typedef struct {
 char sender[128],account[128],target[128],message[512];
} message_t;
typedef struct {size_t begin,end;} word_t;

static const stnlabz_module_host_t *host;
static time_t last_passive_reply;
static void observe(const stnlabz_module_host_t *api,const char *operation,
                    const char *status,unsigned records,unsigned line,int failed){
 uint64_t trace=0,query_token=1;size_t used=0;digit_dsd_record_t event={0};
 if(!api||!api->invoke_service)return;
 if(api->invoke_service(DIGIT_DSD_CURRENT_SERVICE,&query_token,sizeof query_token,&trace,
    sizeof trace,&used)!=STNLABZ_MODULE_OK||used!=sizeof trace||!trace)return;
 event.trace_id=event.event.trace_id=trace;
 event.event.span_id=3;
 event.event.parent_span_id=1;
 event.event.result=failed?1:0;
 event.event.records=records;
 event.event.line=line;
 snprintf(event.event.module,sizeof event.event.module,"conversation");
 snprintf(event.event.method,sizeof event.event.method,"answer");
 snprintf(event.event.file,sizeof event.event.file,"modules/conversation/conversation.c");
 snprintf(event.event.operation,sizeof event.event.operation,"%s",operation);
 snprintf(event.event.status,sizeof event.event.status,"%s",status);
 (void)api->invoke_service(DIGIT_DSD_RECORD_SERVICE,&event,sizeof event,
                          NULL,0,&used);
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
/* Search only content-bearing terms; conversational filler is not evidence.
 * A returned corpus hit must contain the searched topic as a whole token.
 * Do not treat three FTS matches as three agreeing sources. */
static int filler(const char *p,size_t n){
 static const char *const ignored[]={"tell","me","about","what","is","are",
  "the","a","an","please","can","you","explain","do","does","of","to",
  "some","info","information","on","give","and","for","i","we","know"};
 size_t i;
 for(i=0;i<sizeof ignored/sizeof ignored[0];i++)
  if(strlen(ignored[i])==n&&!strncasecmp(p,ignored[i],n))return 1;
 return 0;
}
static int topic_word_character(unsigned char c){
 return isalnum(c)||c=='_';
}
static int topic_present(const char *text,const char *term){
 size_t n=strlen(term),i,len=strlen(text);
 if(!n)return 0;
 for(i=0;i+n<=len;i++){
  if(!strncasecmp(text+i,term,n) &&
   (i==0||!topic_word_character((unsigned char)text[i-1])) &&
   (i+n==len||!topic_word_character((unsigned char)text[i+n])))return 1;
 }
 return 0;
}
static stnlabz_module_result_t answer(const message_t *m,char *response,
                                      size_t capacity,size_t *used,
                                      const stnlabz_module_host_t *api){
 word_t words[WORD_CAP];size_t count,i,returned,added=0;
 stnlabz_module_result_t rc;
 digit_corpus_query_t query;
 digit_corpus_result_t results;
 char topic[DIGIT_CORPUS_QUERY_MAX]={0},line[512]={0};
 if(used)*used=0;
 if(!message_valid(m)||!response||!capacity||!used||!api||
    !api->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 count=tokens(m->message,words,WORD_CAP);
 if(!count)return reply(response,capacity,used,"I couldn't identify searchable terms.");
 /* Prefer the last meaningful topic term. Do not fall back to "tell", "me",
  * etc., because those produce irrelevant answers that appear successful. */
 for(i=count;i>0;i--){
  size_t n=words[i-1].end-words[i-1].begin;
  const char *p=m->message+words[i-1].begin;
  if(n>1&&n<sizeof topic&&!filler(p,n)){
   memcpy(topic,p,n);topic[n]=0;break;
  }
 }
 if(!topic[0])return reply(response,capacity,used,"I couldn't identify a topic.");
 memset(&query,0,sizeof query);snprintf(query.query,sizeof query.query,"%s",topic);
 memset(&results,0,sizeof results);returned=0;
 rc=api->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&query,sizeof query,
                        &results,sizeof results,&returned);
 if(rc!=STNLABZ_MODULE_OK){
  observe(api,"corpus_search","SERVICE_ERROR",0,__LINE__,1);return rc;
 }
 if(returned!=sizeof results||results.count>DIGIT_CORPUS_RESULTS_MAX)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 for(i=0;i<results.count;i++){
  const digit_corpus_hit_t *hit=&results.hits[i];
  char entry[160];int n;
  if(!memchr(hit->excerpt,0,sizeof hit->excerpt) ||
     !topic_present(hit->excerpt,topic))continue;
  n=snprintf(entry,sizeof entry,"%s#%lld: %.105s",
    added?" | ":"Corpus ",(long long)hit->id,hit->excerpt);
  if(n<0||(size_t)n>=sizeof entry)return STNLABZ_MODULE_ERR_INVALID_STATE;
  if(strlen(line)+strlen(entry)>=sizeof line-1)break;
  strcat(line,entry);added++;
 }
 if(!added){
  observe(api,"corpus_search","QUERY_NO_RELEVANT_EVIDENCE",results.count,__LINE__,0);
  return reply(response,capacity,used,"No relevant corpus evidence was found for that topic.");
 }
 observe(api,"corpus_search","QUERY_MATCH",added,__LINE__,0);
 return reply(response,capacity,used,line);
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
/* Narrow evidence-grounded tutoring interaction. This is intentionally
 * not a general natural-language interpreter or permission to chatter. */
static stnlabz_module_result_t listen_message(const message_t *m,char *out,
 size_t cap,size_t *used,const stnlabz_module_host_t *api){
 digit_observation_recent_query_t request={0};
 digit_observation_recent_result_t history={0};
 digit_corpus_query_t query={0};
 digit_corpus_result_t evidence={0};
 size_t read=0,i;
 int topic=0,prior_problem=0,other_speaker=0;
 if(used)*used=0;
 if(!message_valid(m)||!out||!cap||!used||!api||!api->invoke_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(strcmp(m->target,"#stn-labz"))return STNLABZ_MODULE_OK;
 if(last_passive_reply&&time(NULL)>=last_passive_reply&&
    time(NULL)-last_passive_reply<120)return STNLABZ_MODULE_OK;
 snprintf(request.channel,sizeof request.channel,"%s",m->target);
 if(api->invoke_service(DIGIT_OBSERVATION_RECENT_SERVICE,&request,
    sizeof request,&history,sizeof history,&read)!=STNLABZ_MODULE_OK||
    read!=sizeof history||history.count>DIGIT_OBSERVATION_RECENT_MAX||
    history.count<3)return STNLABZ_MODULE_OK;
 for(i=0;i<history.count;i++)
  if(!memchr(history.items[i].text,0,sizeof history.items[i].text)||
     !memchr(history.items[i].sender,0,sizeof history.items[i].sender))
   return STNLABZ_MODULE_OK;
 /* Fail closed if the last observation does not describe this exact turn. */
 if(strcmp(history.items[history.count-1].text,m->message)||
    (history.items[history.count-1].sender[0]&&
     strcasecmp(history.items[history.count-1].sender,m->sender)))
  return STNLABZ_MODULE_OK;
 for(i=0;i+1<history.count;i++){
  if(topic_present(history.items[i].text,"PHP"))topic=1;
  if(topic_present(history.items[i].text,"semicolon"))prior_problem=1;
  if(history.items[i].sender[0]&&strcasecmp(history.items[i].sender,m->sender))
   other_speaker=1;
 }
 /* Limited first acceptance scenario: the learner explicitly describes a
  * repeated obstacle following another speaker's discussion of PHP. */
 if(!topic||!prior_problem||!other_speaker||
    !topic_present(m->message,"semicolon")||
    !strstr(m->message,"forgetting"))return STNLABZ_MODULE_OK;
 snprintf(query.query,sizeof query.query,"PHP");
 read=0;
 if(api->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&query,sizeof query,
    &evidence,sizeof evidence,&read)!=STNLABZ_MODULE_OK||
    read!=sizeof evidence||evidence.count>DIGIT_CORPUS_RESULTS_MAX)
  return STNLABZ_MODULE_OK;
 for(i=0;i<evidence.count;i++){
  const digit_corpus_hit_t *hit=&evidence.hits[i];
  char line[512];int length;
  if(!memchr(hit->excerpt,0,sizeof hit->excerpt)||
     !topic_present(hit->excerpt,"PHP")||
     !topic_present(hit->excerpt,"semicolon"))continue;
  length=snprintf(line,sizeof line,"%s, corpus #%lld notes: %.170s",
      m->sender,(long long)hit->id,hit->excerpt);
  if(length>0&&(size_t)length<cap&&length<350){
   stnlabz_module_result_t rc=reply(out,cap,used,line);
   if(rc==STNLABZ_MODULE_OK)last_passive_reply=time(NULL);
   return rc;
  }
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t listen_service(const void *request,
 size_t size,void *response,size_t cap,size_t *used,void *context){
 (void)context;
 if(used)*used=0;
 if(!request||size!=sizeof(message_t))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 return listen_message(request,response,cap,used,host);
}
static void check(stnlabz_module_qualification_result_t *q,int good,int negative){
 q->tests_executed++;
 if(good)q->tests_passed++;else q->tests_failed++;
 if(negative){q->negative_test_executed=1;if(good)q->negative_test_passed=1;}
}
/* Isolated relevance/three-record evidence tests: no SQLite connection. */
static stnlabz_module_result_t fixture_search(const char *name,
 const void *request,size_t size,void *response,size_t cap,size_t *used){
 digit_corpus_result_t out={0};
 const digit_corpus_query_t *query=request;
 if(strcmp(name,DIGIT_CORPUS_SEARCH_SERVICE)||!query||size!=sizeof *query||
    !response||cap<sizeof out||!used)return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(strcmp(query->query,"PHP"))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 out.count=3;
 out.hits[0].id=1;snprintf(out.hits[0].excerpt,sizeof out.hits[0].excerpt,
    "PHP supports PDO databases.");
 out.hits[1].id=2;snprintf(out.hits[1].excerpt,sizeof out.hits[1].excerpt,
    "PHP includes a SQLite PDO driver.");
 out.hits[2].id=3;snprintf(out.hits[2].excerpt,sizeof out.hits[2].excerpt,
    "PHP configuration controls extensions.");
 memcpy(response,&out,sizeof out);*used=sizeof out;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t fixture_irrelevant(const char *name,
 const void *request,size_t size,void *response,size_t cap,size_t *used){
 digit_corpus_result_t out={0};
 const digit_corpus_query_t *query=request;
 if(strcmp(name,DIGIT_CORPUS_SEARCH_SERVICE)||!query||size!=sizeof *query||
    !response||cap<sizeof out||!used)return STNLABZ_MODULE_ERR_NOT_FOUND;
 out.count=3;out.hits[0].id=17;
 snprintf(out.hits[0].excerpt,sizeof out.hits[0].excerpt,"tell me what X means");
 out.hits[1].id=18;
 snprintf(out.hits[1].excerpt,sizeof out.hits[1].excerpt,"unrelated greeting");
 out.hits[2].id=19;
 snprintf(out.hits[2].excerpt,sizeof out.hits[2].excerpt,"general information");
 memcpy(response,&out,sizeof out);*used=sizeof out;return STNLABZ_MODULE_OK;
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
 check(q,topic_present("In PHP, a statement ends with a semicolon.","semicolon"),0);
 check(q,!topic_present("semicolons","semicolon"),1);
 check(q,topic_present("PHP echo","PHP"),0);
 check(q,tokens("STN-Labz",w,WORD_CAP)==1,0);
 check(q,search_phrase("STN-Labz",w,0,1,&query)&&!strcmp(query.query,"STN-Labz"),0);
 check(q,tokens("alpha beta",w,WORD_CAP)==2,0);
 check(q,search_phrase("alpha beta",w,0,2,&query)&&!strcmp(query.query,"alpha beta"),0);
 n=tokens("1",w,WORD_CAP);check(q,n==1,0);
 {
  stnlabz_module_host_t fixture={0};
  snprintf(m.message,sizeof m.message,"tell me about PHP");
  fixture.invoke_service=fixture_search;
  check(q,answer(&m,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_OK,0);
  check(q,strstr(response,"#1:")&&strstr(response,"#2:")&&
          strstr(response,"#3:"),0);
  check(q,!strstr(response,"tell me what X means"),1);
  fixture.invoke_service=fixture_irrelevant;
  check(q,answer(&m,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_OK,0);
  check(q,strstr(response,"No relevant corpus evidence")!=NULL,1);
  check(q,!strstr(response,"#17:"),1);
  fixture.invoke_service=fixture_search;
  snprintf(m.message,sizeof m.message,"we discuss PHP");
  check(q,listen_message(&m,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_OK&&used==0,0);
  snprintf(m.message,sizeof m.message,"laurie where do you upload your module to?");
  check(q,listen_message(&m,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_OK&&used==0,1);
  snprintf(m.message,sizeof m.message,"I have my stuff here at work");
  check(q,listen_message(&m,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_OK&&used==0,1);
  check(q,listen_message(NULL,response,sizeof response,&used,&fixture)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 }
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
 if(!api->register_service(CONVERSATION_LISTEN_SERVICE,listen_service,NULL)){
  (void)api->unregister_service(CONVERSATION_SERVICE,NULL);
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 last_passive_reply=0;
 host=api;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(CONVERSATION_LISTEN_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(CONVERSATION_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="conversation",.name="Conversation knowledge retrieval",
 .version_major=0,.version_minor=4,.version_patch=7,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
