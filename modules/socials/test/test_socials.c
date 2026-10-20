#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "socials.h"
#include "../sqlite3/corpus_search.h"

static int corpus_fixture_mode;
static stnlabz_module_result_t mock_invoke(const char *name,
 const void *request,size_t request_size,void *response,
 size_t capacity,size_t *used){
 digit_corpus_result_t result={0};
 const digit_corpus_query_t *query=request;
 if(used)*used=0;
 if(strcmp(name,DIGIT_CORPUS_SEARCH_SERVICE)||!request||
    request_size!=sizeof *query||!response||!used||
    capacity<sizeof result)return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(!corpus_fixture_mode)return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(strcmp(query->query,"feline"))return STNLABZ_MODULE_ERR_NOT_FOUND;
 result.count=1;result.hits[0].id=42;
 strcpy(result.hits[0].category,corpus_fixture_mode==2?
   "engineering":"feline");
 strcpy(result.hits[0].verification_status,
   corpus_fixture_mode==3?"unverified":"operator-verified");
 strcpy(result.hits[0].excerpt,corpus_fixture_mode==4?
   "A very long description of a kitten phrase, not an expression":
   "mrrp");
 memcpy(response,&result,sizeof result);
 *used=sizeof result;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_service_handler_fn callback;
static stnlabz_module_service_handler_fn greeting_callback;
static stnlabz_module_service_handler_fn family_callback;
static void *registered_context;
static int registrations;
static int reg(const char *name,stnlabz_module_service_handler_fn fn,void *ctx) {
 if(!fn)return 0;
 if(!strcmp(name,DIGIT_SOCIALS_SERVICE)&&!callback){
  callback=fn;registered_context=ctx;registrations++;return 1;
 }
 if(!strcmp(name,DIGIT_SOCIALS_FAMILY_SERVICE)&&!family_callback){
  family_callback=fn;registrations++;return 1;
 }
 if(!strcmp(name,DIGIT_SOCIALS_GREETING_SERVICE)&&!greeting_callback){
  greeting_callback=fn;registrations++;return 1;
 }
 return 0;
}
static int unreg(const char *name,void *ctx) {
 if(!strcmp(name,DIGIT_SOCIALS_SERVICE)&&callback&&ctx==registered_context){
  callback=NULL;return 1;
 }
 if(!strcmp(name,DIGIT_SOCIALS_FAMILY_SERVICE)&&family_callback&&ctx==NULL){
  family_callback=NULL;return 1;
 }
 if(!strcmp(name,DIGIT_SOCIALS_GREETING_SERVICE)&&greeting_callback&&ctx==NULL){
  greeting_callback=NULL;return 1;
 }
 return 0;
}
static void demand(int condition,const char *what) {
 if(!condition){fprintf(stderr,"FAIL: %s\n",what);exit(1);}
}
int main(int argc,char **argv) {
 void *library;
 const stnlabz_module_descriptor_t *(*factory)(void);
 const stnlabz_module_descriptor_t *d;
 stnlabz_module_qualification_result_t qualification={0};
 stnlabz_module_host_t api={0};
 const char *categories[]={"hearts","smiles","cats","paws","playful",
     "comfort","sad","celebrate","gestures","text"};
 size_t i,used;
 digit_socials_request_t request={0};
 digit_socials_response_t output={0};
 char previous[DIGIT_SOCIALS_EXPRESSION_MAX];
 if(argc!=2)return 2;
 library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 demand(library!=NULL,"dlopen");
 *(void **)(&factory)=dlsym(library,"stnlabz_module_get_descriptor");
 demand(factory!=NULL,"factory");
 d=factory();
 demand(d&&strcmp(d->id,"socials")==0,"identity");
 demand(d->required_core_api_major==1&&d->required_core_api_minor==5,"ABI");
 demand(d->qualify(&qualification)==STNLABZ_MODULE_OK,"qualification");
 demand(qualification.tests_executed>=20&&qualification.tests_failed==0,
        "qualification checks");
 api.register_service=reg;api.unregister_service=unreg;
 api.invoke_service=mock_invoke;
 demand(d->start(&api)==STNLABZ_MODULE_OK,"start");
 demand(registrations==3&&callback!=NULL&&greeting_callback!=NULL&&family_callback!=NULL,"registration");
 for(i=0;i<sizeof categories/sizeof categories[0];i++){
  unsigned trial;
  memset(&request,0,sizeof request);
  memcpy(request.category,categories[i],strlen(categories[i]));
  memset(previous,0,sizeof previous);
  for(trial=0;trial<12;trial++){
   memset(&output,0,sizeof output);used=0;
   demand(callback(&request,sizeof request,&output,sizeof output,&used,
       registered_context)==STNLABZ_MODULE_OK,"selection");
   demand(used==sizeof output && output.expression[0] &&
       memchr(output.expression,0,sizeof output.expression)!=NULL,"UTF8 response");
   demand(strcmp(previous,output.expression)!=0,"no immediate repetition");
   memcpy(previous,output.expression,sizeof previous);
  }
 }
 /* Corpus may enrich expression vocabulary for any caller, but only
  * verified, category-labeled, short expressions may be emitted. */
 memset(&request,0,sizeof request);
 strcpy(request.category,"cats");
 corpus_fixture_mode=1;used=0;
 demand(callback(&request,sizeof request,&output,sizeof output,&used,
    registered_context)==STNLABZ_MODULE_OK&&
    !strcmp(output.expression,"mrrp"),"verified Corpus feline expression");
 used=0;
 demand(callback(&request,sizeof request,&output,sizeof output,&used,
    registered_context)==STNLABZ_MODULE_OK&&
    strcmp(output.expression,"mrrp"),"Corpus expression no immediate repeat");
 corpus_fixture_mode=2;used=0;
 demand(callback(&request,sizeof request,&output,sizeof output,&used,
    registered_context)==STNLABZ_MODULE_OK&&
    strcmp(output.expression,"mrrp"),"reject incorrect Corpus category");
 corpus_fixture_mode=3;used=0;
 demand(callback(&request,sizeof request,&output,sizeof output,&used,
    registered_context)==STNLABZ_MODULE_OK&&
    strcmp(output.expression,"mrrp"),"reject unverified Corpus record");
 corpus_fixture_mode=4;used=0;
 demand(callback(&request,sizeof request,&output,sizeof output,&used,
    registered_context)==STNLABZ_MODULE_OK&&
    strcmp(output.expression,"mrrp"),"reject narrative Corpus excerpt");
 corpus_fixture_mode=0;
 memset(&request,0,sizeof request);
 strcpy(request.category,"unknown");
 demand(callback(&request,sizeof request,&output,sizeof output,&used,NULL)==
        STNLABZ_MODULE_ERR_NOT_FOUND,"unknown category");
 {digit_socials_greeting_request_t greeting={0};
  digit_socials_greeting_response_t result={0};
  const char *kinds[]={"mommy","auntie","sister","known","unknown"};
  for(i=0;i<sizeof kinds/sizeof kinds[0];i++){
   memset(&greeting,0,sizeof greeting);
   strcpy(greeting.kind,kinds[i]);used=0;
   demand(greeting_callback(&greeting,sizeof greeting,&result,sizeof result,
     &used,NULL)==STNLABZ_MODULE_OK,"greeting selection");
   demand(used==sizeof result&&result.text[0],"greeting output");
   if(i==0)demand(strstr(result.text,"Mommy")!=NULL,"mommy address");
   if(i==1)demand(strstr(result.text,"Auntie")!=NULL,"auntie address");
   if(i==2)demand(strstr(result.text,"Ezra")!=NULL,"sister address");
   used=0;
   demand(greeting_callback(&greeting,sizeof greeting,&result,sizeof result,
     &used,NULL)==STNLABZ_MODULE_ERR_INVALID_STATE&&used==0,"cooldown");
  }
  memset(&greeting,0,sizeof greeting);
  strcpy(greeting.kind,"untrusted");
  demand(greeting_callback(&greeting,sizeof greeting,&result,sizeof result,
    &used,NULL)==STNLABZ_MODULE_ERR_NOT_FOUND,"invalid relationship");
 }
 {
  digit_socials_family_request_t family={0};
  digit_socials_family_response_t reply={0};
  const char *kinds[]={"mommy","auntie","sister"};
  const char *names[]={"Momma","Auntie","Kitty Sister"};
  const char *phrases[]={"good to see you","love you","hello","thanks","there you are kitten","awww Digit :)"};
  size_t role,intent;
  for(role=0;role<3;role++){
   for(intent=0;intent<sizeof phrases/sizeof phrases[0];intent++){
    memset(&family,0,sizeof family);
    strcpy(family.kind,kinds[role]);
    strcpy(family.text,phrases[intent]);
    used=0;
    demand(family_callback(&family,sizeof family,&reply,sizeof reply,
      &used,NULL)==STNLABZ_MODULE_OK,"family interaction");
    demand(used==sizeof reply&&strstr(reply.text,names[role])!=NULL,
      "family address");
   }
  }
  memset(&family,0,sizeof family);
  strcpy(family.kind,"auntie");strcpy(family.text,"what is C?");
  used=99;
  demand(family_callback(&family,sizeof family,&reply,sizeof reply,
    &used,NULL)==STNLABZ_MODULE_ERR_NOT_FOUND&&used==0,
    "factual question not consumed");
  strcpy(family.text,":)");
  used=0;
  demand(family_callback(&family,sizeof family,&reply,sizeof reply,
    &used,NULL)==STNLABZ_MODULE_OK&&used==sizeof reply&&
    strstr(reply.text,"Auntie")!=NULL,
    "smiley-only family acknowledgment");
  strcpy(family.text,"hey what is C?");
  used=99;
  demand(family_callback(&family,sizeof family,&reply,sizeof reply,
    &used,NULL)==STNLABZ_MODULE_ERR_NOT_FOUND&&used==0,
    "mixed technical question not consumed");
  strcpy(family.kind,"unknown");strcpy(family.text,"love you");
  used=99;
  demand(family_callback(&family,sizeof family,&reply,sizeof reply,
    &used,NULL)==STNLABZ_MODULE_ERR_NOT_FOUND&&used==0,
    "nonfamily denied");
  memset(family.text,'X',sizeof family.text);
  strcpy(family.kind,"auntie");used=99;
  demand(family_callback(&family,sizeof family,&reply,sizeof reply,
    &used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT&&used==0,
    "nonterminated message rejected");
 }
 demand(d->stop()==STNLABZ_MODULE_OK,"stop");
 demand(callback==NULL&&greeting_callback==NULL&&family_callback==NULL,"unregistration");
 dlclose(library);
 printf("socials qualification %u/%u; service tests PASS\n",
        qualification.tests_passed,qualification.tests_executed);
 return 0;
}
