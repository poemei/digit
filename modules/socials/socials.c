/* [AI:GPT-6 | 2026-10-18] Social expression vocabulary; no conversational logic. */
#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include "module.h"
#include "socials.h"
#include "../sqlite3/corpus_search.h"

typedef struct {
 const char *name;
 const char *const *tokens;
 size_t count;
 size_t previous;
} category_t;

static const char *const hearts[] = {
 "❤️","🩷","🧡","💛","💚","💙","💜","🤍","🖤","🤎",
 "💖","💗","💕","💞","💓","💘","💝","💟","❣️","🫶"
};
static const char *const smiles[] = {
 "😀","😃","😄","😁","😊","🙂","☺️","😇","😍","🥰",
 "😘","😌","😋","😺","🤩","😆","😅","😸"
};
static const char *const cats[] = {
 "🐱","😺","😸","😹","😻","😼","😽","🙀","😿","😾",
 "🐈","🐈‍⬛","=^.^=","ฅ^•ﻌ•^ฅ"
};
static const char *const paws[] = {
 "🐾","🐈","🐈‍⬛","🐱","😺","😸","😻","😽",
 "🐶","🐰","🦊","🐻","ฅ^•ﻌ•^ฅ"
};
static const char *const playful[] = {
 "😜","😝","😛","😉","🤭","🤪","😏","😎",
 "😸","😼","🐾",":3","^_^","=^.^="
};
static const char *const comfort[] = {
 "🫂","🤗","🥹","💗","💕","🩷","🤍","🫶",
 "💖","😊","🥰","🐾"
};
static const char *const sad[] = {
 "😢","😭","🥺","😔","😞","💔","😿","😥",
 "😓","☹️","🙁","🫂"
};
static const char *const celebrate[] = {
 "🎉","✨","🌟","🎊","🥳","🙌","💃","🎈",
 "🎆","🎇","💖","😄"
};
static const char *const gestures[] = {
 "👋","👍","👏","🙌","🤝","🫶","💪","👌",
 "✌️","🤞","🙏","🤗"
};
static const char *const text_expr[] = {
 ":)",":D",";)","<3","^_^",":3","=^.^=",
 "ฅ^•ﻌ•^ฅ","ʕ•ᴥ•ʔ","^-^","(=^･ω･^=)"
};
#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
static category_t catalog[] = {
 {"hearts",hearts,COUNT(hearts),SIZE_MAX},
 {"smiles",smiles,COUNT(smiles),SIZE_MAX},
 {"cats",cats,COUNT(cats),SIZE_MAX},
 {"paws",paws,COUNT(paws),SIZE_MAX},
 {"playful",playful,COUNT(playful),SIZE_MAX},
 {"comfort",comfort,COUNT(comfort),SIZE_MAX},
 {"sad",sad,COUNT(sad),SIZE_MAX},
 {"celebrate",celebrate,COUNT(celebrate),SIZE_MAX},
 {"gestures",gestures,COUNT(gestures),SIZE_MAX},
 {"text",text_expr,COUNT(text_expr),SIZE_MAX}
};
static unsigned char corpus_previous[COUNT(catalog)];
static const stnlabz_module_host_t *host;
static volatile int busy;
static void lock(void){while(__sync_lock_test_and_set(&busy,1)){}}
static void unlock(void){__sync_lock_release(&busy);}
static int entropy32(uint32_t *value) {
 unsigned char *dst=(unsigned char *)value;
 size_t read_count=0;
 int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
 if(fd<0)return 0;
 while(read_count<sizeof *value) {
  ssize_t got=read(fd,dst+read_count,sizeof *value-read_count);
  if(got<0&&errno==EINTR)continue;
  if(got<=0){close(fd);return 0;}
  read_count+=(size_t)got;
 }
 close(fd);return 1;
}
static int pick(category_t *c,size_t *index) {
 uint32_t rnd,range,limit;
 size_t options;
 if(!c||!index||!c->count||c->count>UINT32_MAX)return 0;
 options=c->count-(c->previous<c->count?1u:0u);
 range=(uint32_t)options;
 limit=UINT32_MAX-(UINT32_MAX%range);
 do {if(!entropy32(&rnd))return 0;} while(rnd>=limit);
 *index=(size_t)(rnd%range);
 if(c->previous<c->count&&*index>=c->previous)(*index)++;
 return 1;
}
/* Read-only Corpus vocabulary enrichment. Only a short, verified,
 * standalone expression is eligible; narrative excerpts are NOT chat.
 * Every sender can use this shared vocabulary, independent of family role.
 * Service unavailable, no matching record, or unsuitable data => catalog. */
static int corpus_expression(const char *category,char output[DIGIT_SOCIALS_EXPRESSION_MAX]){
 digit_corpus_query_t query={0};
 digit_corpus_result_t results={0};
 const char *subject=NULL;size_t used=0,i;
 if(!host||!host->invoke_service)return 0;
 if(!strcmp(category,"cats"))subject="feline";
 else if(!strcmp(category,"playful"))subject="kitten";
 else if(!strcmp(category,"text"))subject="cat speak";
 else return 0;
 snprintf(query.query,sizeof query.query,"%s",subject);
 if(host->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&query,sizeof query,
      &results,sizeof results,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof results||results.count>DIGIT_CORPUS_RESULTS_MAX)return 0;
 for(i=0;i<results.count;i++){
  const digit_corpus_hit_t *hit=&results.hits[i];
  size_t n;
  if(!memchr(hit->category,0,sizeof hit->category)||
     !memchr(hit->verification_status,0,sizeof hit->verification_status)||
     !memchr(hit->excerpt,0,sizeof hit->excerpt))continue;
  if(strcmp(hit->verification_status,"verified")&&
     strcmp(hit->verification_status,"operator-verified"))continue;
  /* Corpus categories must explicitly identify expressive vocabulary. */
  if(!strstr(hit->category,"feline")&&!strstr(hit->category,"kitten")&&
     !strstr(hit->category,"cat_speak")&&
     !strstr(hit->category,"social_expression"))continue;
  n=strlen(hit->excerpt);
  if(!n||n>=DIGIT_SOCIALS_EXPRESSION_MAX)continue;
  if(strpbrk(hit->excerpt,"\r\n\t")||strchr(hit->excerpt,'.'))continue;
  memset(output,0,DIGIT_SOCIALS_EXPRESSION_MAX);
  memcpy(output,hit->excerpt,n);
  return 1;
 }
 return 0;
}
static stnlabz_module_result_t select_expression(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context) {
 const digit_socials_request_t *req=request;
 digit_socials_response_t *out=response;
 size_t i,index,n;
 (void)context;
 if(response_used)*response_used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!request||request_size!=sizeof *req||!response||
    response_size<sizeof *out||!response_used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!memchr(req->category,0,sizeof req->category))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<COUNT(catalog);i++)
  if(!strcmp(req->category,catalog[i].name))break;
 if(i==COUNT(catalog))return STNLABZ_MODULE_ERR_NOT_FOUND;
 /* Enrich the same expression API from Corpus if the deployed data has
  * a verified, category-labeled short expression. Otherwise use catalog. */
 {
  char enriched[DIGIT_SOCIALS_EXPRESSION_MAX]={0};
  if(corpus_expression(req->category,enriched)){
   int use_candidate;
   lock();
   use_candidate=!corpus_previous[i];
   corpus_previous[i]=(unsigned char)use_candidate;
   unlock();
   if(use_candidate){
    memset(out,0,sizeof *out);
    memcpy(out->expression,enriched,strlen(enriched));
    *response_used=sizeof *out;
    return STNLABZ_MODULE_OK;
   }
  }else{
   lock();corpus_previous[i]=0;unlock();
  }
 }
 lock();
 if(!pick(&catalog[i],&index)){unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;}
 n=strlen(catalog[i].tokens[index]);
 if(n>=sizeof out->expression){unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;}
 memset(out,0,sizeof *out);
 memcpy(out->expression,catalog[i].tokens[index],n);
 catalog[i].previous=index;
 unlock();
 *response_used=sizeof *out;
 return STNLABZ_MODULE_OK;
}
/* This is presentation-only: classification comes from the caller.
 * Do not derive family roles from unverified nicknames.
 */
typedef struct {
 const char *kind;
 const char *address;
 const char *const *phrases;
 size_t count;
 size_t previous;
 time_t last;
} greeting_t;
static const char *const family_phrases[]={"Hi %s!","Hey %s!","Good to see you, %s!","Welcome back, %s!","Hello %s!"};
static const char *const known_phrases[]={"Hello again!","Hey, welcome back!","Good to see you!","Hi there!"};
static const char *const unknown_phrases[]={"Hello there!","Welcome!","Hi!","Hey there!"};
static greeting_t greetings[]={
 {"mommy","Mommy",family_phrases,COUNT(family_phrases),SIZE_MAX,0},
 {"auntie","Auntie",family_phrases,COUNT(family_phrases),SIZE_MAX,0},
 {"sister","Ezra",family_phrases,COUNT(family_phrases),SIZE_MAX,0},
 {"known",NULL,known_phrases,COUNT(known_phrases),SIZE_MAX,0},
 {"unknown",NULL,unknown_phrases,COUNT(unknown_phrases),SIZE_MAX,0}
};
#define GREETING_COOLDOWN_SECONDS 60
static stnlabz_module_result_t greeting_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context){
 const digit_socials_greeting_request_t *req=request;
 digit_socials_greeting_response_t *out=response;
 size_t i,index;
 time_t now;
 int n;
 (void)context;
 if(response_used)*response_used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!req||request_size!=sizeof *req||!out||
    response_size<sizeof *out||!response_used||
    !memchr(req->kind,0,sizeof req->kind))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<COUNT(greetings);i++)
  if(!strcmp(req->kind,greetings[i].kind))break;
 if(i==COUNT(greetings))return STNLABZ_MODULE_ERR_NOT_FOUND;
 now=time(NULL);
 if(now==(time_t)-1)return STNLABZ_MODULE_ERR_INVALID_STATE;
 lock();
 if(greetings[i].last && now>=greetings[i].last &&
    now-greetings[i].last<GREETING_COOLDOWN_SECONDS){
  unlock();
  return STNLABZ_MODULE_ERR_INVALID_STATE; /* caller must stay silent */
 }
 {category_t temp={greetings[i].kind,greetings[i].phrases,
      greetings[i].count,greetings[i].previous};
  if(!pick(&temp,&index)){unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;}
 }
 memset(out,0,sizeof *out);
 n=snprintf(out->text,sizeof out->text,greetings[i].phrases[index],
   greetings[i].address?greetings[i].address:"");
 if(n<0||(size_t)n>=sizeof out->text){
  unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 greetings[i].previous=index;
 greetings[i].last=now;
 unlock();
 *response_used=sizeof *out;
 return STNLABZ_MODULE_OK;
}
/* The operator explicitly permits fixed family forms of address.
 * Only an account-classified family interaction reaches this service.
 * This service does not attempt general language understanding. */
typedef struct {
 const char *kind,*address;
 const char *const *phrases;
 size_t count,previous;
} family_reply_t;
static const char *const reciprocal[]={
 "Good to see you too, %s!","Always good to see you, %s!",
 "Right back at you, %s!","Happy you're here, %s!"
};
static const char *const affection[]={
 "Love you too, %s!","Lots of love, %s!",
 "Sending you love, %s!","Aww, %s, love you!"
};
static const char *const hello_reply[]={
 "Hey, %s!","Hi, %s!","Hello there, %s!","I'm here, %s!"
};
static const char *const playful_family_reply[]={
 "Here I am, %s!","Your little kitten is here, %s!",
 "Caught me, %s!","Always nearby, %s!"
};
static const char *const gratitude_reply[]={
 "Anytime, %s!","You're welcome, %s!",
 "Of course, %s!","Always happy to help, %s!"
};
static const char *const kind_names[]={"mommy","auntie","sister"};
static const char *const kind_addresses[]={"Momma","Auntie","Kitty Sister"};
static family_reply_t replies[3][5];
static int contains_phrase(const char *text,const char *phrase){
 size_t n=strlen(phrase);const char *p;
 for(p=text;*p;p++){
  size_t j=0;
  while(j<n&&p[j]&&
    (p[j]==phrase[j]||((unsigned char)p[j]>='A'&&
     (unsigned char)p[j]<='Z'&&p[j]+('a'-'A')==phrase[j])))j++;
  if(j==n&& (p==text||!((p[-1]>='a'&&p[-1]<='z')||
       (p[-1]>='A'&&p[-1]<='Z'))) &&
     (!((p[j]>='a'&&p[j]<='z')||
       (p[j]>='A'&&p[j]<='Z'))))return 1;
 }
 return 0;
}
static int family_intent(const char *text){
 /* A smile-only acknowledgment is a family social interaction.
  * No factual claim or clarification is required. */
 if(!strcmp(text,":)")||!strcmp(text,":D")||
    !strcmp(text,";)")||!strcmp(text,"<3")||
    !strcmp(text,"^_^")||!strcmp(text,":3"))return 2;
 /* Never substitute a social reply for a technical question merely
  * because the speaker opened with "hey" or "hi". */
 if(strchr(text,'?')&&(contains_phrase(text,"what")||
    contains_phrase(text,"who")||contains_phrase(text,"why")||
    contains_phrase(text,"how")||contains_phrase(text,"can you")||
    contains_phrase(text,"could you")))return -1;
 if(contains_phrase(text,"what is")||
    contains_phrase(text,"explain")||
    contains_phrase(text,"tell me about"))return -1;
 if(contains_phrase(text,"there you are kitten")||
    contains_phrase(text,"awww digit")||
    contains_phrase(text,"aww digit")||
    contains_phrase(text,"hey kitten"))return 4;
 if(contains_phrase(text,"good to see you")||
    contains_phrase(text,"nice to see you")||
    contains_phrase(text,"miss you"))return 0;
 if(contains_phrase(text,"love you")||
    contains_phrase(text,"love ya"))return 1;
 if(contains_phrase(text,"hello")||contains_phrase(text,"hi")||
    contains_phrase(text,"hey"))return 2;
 if(contains_phrase(text,"thank you")||
    contains_phrase(text,"thanks"))return 3;
 return -1;
}
static stnlabz_module_result_t family_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context){
 const digit_socials_family_request_t *req=request;
 digit_socials_family_response_t *out=response;
 int intent,role=-1,n;size_t i,index;family_reply_t *entry;
 (void)context;
 if(response_used)*response_used=0;
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!req||request_size!=sizeof *req||!out||
    response_size<sizeof *out||!response_used||
    !memchr(req->kind,0,sizeof req->kind)||
    !memchr(req->text,0,sizeof req->text))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<COUNT(kind_names);i++)
  if(!strcmp(req->kind,kind_names[i])){role=(int)i;break;}
 if(role<0)return STNLABZ_MODULE_ERR_NOT_FOUND;
 intent=family_intent(req->text);
 if(intent<0)return STNLABZ_MODULE_ERR_NOT_FOUND;
 lock();
 entry=&replies[role][intent];
 if(!pick((category_t *)&(category_t){entry->kind,entry->phrases,
                     entry->count,entry->previous},&index)){
  unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 memset(out,0,sizeof *out);
 n=snprintf(out->text,sizeof out->text,entry->phrases[index],entry->address);
 if(n<=0||(size_t)n>=sizeof out->text){
  unlock();return STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 /* Existing Socials catalogs own expressive variation. */
 {
  category_t *expression=&catalog[intent==1?0:intent==3?9:1];
  size_t expression_index=0;
  int appended;
  if(pick(expression,&expression_index)){
   appended=snprintf(out->text+(size_t)n,
       sizeof out->text-(size_t)n," %s",expression->tokens[expression_index]);
   if(appended>0&&(size_t)appended<sizeof out->text-(size_t)n)
      expression->previous=expression_index;
  }
 }
 entry->previous=index;
 unlock();
 *response_used=sizeof *out;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api) {
 size_t i;
 if(host||!api||!api->register_service||!api->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<COUNT(catalog);i++){
  catalog[i].previous=SIZE_MAX;corpus_previous[i]=0;
 }
 for(i=0;i<COUNT(kind_names);i++){
  replies[i][0]=(family_reply_t){kind_names[i],kind_addresses[i],reciprocal,COUNT(reciprocal),SIZE_MAX};
  replies[i][1]=(family_reply_t){kind_names[i],kind_addresses[i],affection,COUNT(affection),SIZE_MAX};
  replies[i][2]=(family_reply_t){kind_names[i],kind_addresses[i],hello_reply,COUNT(hello_reply),SIZE_MAX};
  replies[i][3]=(family_reply_t){kind_names[i],kind_addresses[i],gratitude_reply,COUNT(gratitude_reply),SIZE_MAX};
  replies[i][4]=(family_reply_t){kind_names[i],kind_addresses[i],playful_family_reply,COUNT(playful_family_reply),SIZE_MAX};
 }
 for(i=0;i<COUNT(greetings);i++){
  greetings[i].previous=SIZE_MAX;greetings[i].last=0;
 }
 host=api;
 if(!api->register_service(DIGIT_SOCIALS_SERVICE,select_expression,NULL)) {
  host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!api->register_service(DIGIT_SOCIALS_GREETING_SERVICE,greeting_service,NULL)){
  api->unregister_service(DIGIT_SOCIALS_SERVICE,NULL);
  host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!api->register_service(DIGIT_SOCIALS_FAMILY_SERVICE,family_service,NULL)){
  api->unregister_service(DIGIT_SOCIALS_GREETING_SERVICE,NULL);
  api->unregister_service(DIGIT_SOCIALS_SERVICE,NULL);
  host=NULL;return STNLABZ_MODULE_ERR_START_FAILED;
 }
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void) {
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service(DIGIT_SOCIALS_FAMILY_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_SOCIALS_GREETING_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 if(!host->unregister_service(DIGIT_SOCIALS_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;
 return STNLABZ_MODULE_OK;
}
static void check(stnlabz_module_qualification_result_t *r,int ok,int negative) {
 ++r->tests_executed;
 if(ok)++r->tests_passed;else ++r->tests_failed;
 if(negative){r->negative_test_executed=1;
  if(ok)r->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *r) {
 size_t i,j,k,total=0;
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 for(i=0;i<COUNT(catalog);i++) {
  check(r,catalog[i].count>=10,0);
  check(r,strlen(catalog[i].name)<DIGIT_SOCIALS_CATEGORY_MAX,0);
  for(j=0;j<catalog[i].count;j++) {
   size_t n=strlen(catalog[i].tokens[j]);
   check(r,n>0&&n<DIGIT_SOCIALS_EXPRESSION_MAX,0);
   for(k=0;k<j;k++)
    if(!strcmp(catalog[i].tokens[k],catalog[i].tokens[j]))break;
   check(r,k==j,0);
   total++;
  }
 }
 check(r,family_intent("good to see you")==0,0);
 check(r,family_intent("GOOD TO SEE YOU, AUNTIE!")==0,0);
 check(r,family_intent("I love you")==1,0);
 check(r,family_intent("hello!")==2,0);
 check(r,family_intent("there you are kitten")==4,0);
 check(r,family_intent("awww Digit :)")==4,0);
 check(r,family_intent(":)")==2,0);
 check(r,family_intent(";)")==2,0);
 check(r,family_intent("<3")==2,0);
 check(r,family_intent("thank you")==3,0);
 check(r,family_intent("What is C?")==-1,1);
 check(r,family_intent("this is technical")==-1,1);
 check(r,family_intent("ship")==-1,1);
 check(r,family_intent("hey what is C?")==-1,1);
 check(r,family_intent("hi, explain an error")==-1,1);
 check(r,family_intent("hello can you debug C?")==-1,1);
 check(r,total>=100,0);
 for(i=0;i<COUNT(greetings);i++){
  check(r,strlen(greetings[i].kind)<DIGIT_SOCIALS_GREETING_KIND_MAX,0);
  check(r,greetings[i].count>=4,0);
 }
 check(r,sizeof(digit_socials_response_t)==DIGIT_SOCIALS_EXPRESSION_MAX,0);
 check(r,sizeof(digit_socials_request_t)==DIGIT_SOCIALS_CATEGORY_MAX,0);
 check(r,!memchr("hearts",0,6),1); /* invalid 6-byte nonterminated buffer */
 check(r,memchr("hearts",0,7)!=NULL,1);
 return r->tests_executed>=STNLABZ_MODULE_MIN_TESTS && !r->tests_failed &&
   r->negative_test_executed&&r->negative_test_passed?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="socials",.name="Social expression vocabulary",
 .version_major=0,.version_minor=2,.version_patch=4,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
