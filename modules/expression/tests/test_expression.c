#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "corpus_store.h"
static stnlabz_module_command_handler_fn handler;
static void *handler_ctx;
static int registered;
static unsigned int executed,failed;
static char output[4608];
static void check(int ok,const char *what){
 executed++;
 if(!ok){failed++;printf("FAIL %s\n",what);}
}
static int register_command_mock(const char *name,stnlabz_module_command_handler_fn h,void *ctx){
 if(strcmp(name,"express")||!h||registered)return 0;
 handler=h;handler_ctx=ctx;registered=1;return 1;
}
static int unregister_command_mock(const char *name,void *ctx){
 if(!registered||strcmp(name,"express")||ctx!=handler_ctx)return 0;
 registered=0;handler=NULL;return 1;
}
static int reply_mock(void *ctx,const char *message){
 (void)ctx;
 if(!message)return 0;
 snprintf(output,sizeof output,"%s",message);
 return 1;
}
static int corpus_find_mock(const char *path,const char *term,digit_corpus_record_t *r){
 if(strcmp(path,DIGIT_CORPUS_PATH))return -1;
 if(!strcmp(term,"fixture-known")){
  memset(r,0,sizeof *r);
  snprintf(r->id,sizeof r->id,"TEST-1");
  snprintf(r->source,sizeof r->source,"test-fixture");
  snprintf(r->text,sizeof r->text,"Evidence from corpus");
  return 1;
 }
 if(!strcmp(term,"fixture-unknown"))return 0;
 return -1;
}
#define digit_corpus_find corpus_find_mock
#include "../src/expression.c"
#undef digit_corpus_find
int main(void){
 stnlabz_module_host_t h={0};
 stnlabz_module_command_t cmd={0};
 stnlabz_module_qualification_result_t q;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 h.register_command=register_command_mock;
 h.unregister_command=unregister_command_mock;
 check(d&&strcmp(d->id,"expression")==0,"descriptor");
 check(d&&d->version_minor==2&&d->version_patch==1,"module version");
 check(d&&d->qualify(&q)==STNLABZ_MODULE_OK,"qualify executes");
 check(q.tests_executed>=STNLABZ_MODULE_MIN_TESTS&&q.tests_failed==0&&
   q.tests_executed==q.tests_passed&&q.negative_test_executed&&q.negative_test_passed,
   "qualification counts reflect executed checks");
 check(d->qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"qualify null rejected");
 check(d->start(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"start null rejected");
 check(d->start(&h)==STNLABZ_MODULE_OK&&registered,"command registration");
 check(d->start(&h)!=STNLABZ_MODULE_OK,"duplicate activation rejected");
 snprintf(cmd.name,sizeof cmd.name,"express");
 snprintf(cmd.arguments,sizeof cmd.arguments,"fixture-known");
 output[0]=0;
 check(handler(&cmd,reply_mock,NULL,handler_ctx)==STNLABZ_MODULE_OK&&
  strstr(output,"Evidence from corpus")&&strstr(output,"record=TEST-1")&&
  strstr(output,"source=test-fixture"),"Corpus evidence reported");
 snprintf(cmd.arguments,sizeof cmd.arguments,"fixture-unknown");
 output[0]=0;
 check(handler(&cmd,reply_mock,NULL,handler_ctx)==STNLABZ_MODULE_OK&&
  strstr(output,"Unknown expression"),"unknown is not invented");
 check(handler(NULL,reply_mock,NULL,handler_ctx)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,
  "invalid command rejected");
 check(d->stop()==STNLABZ_MODULE_OK&&!registered,"unregister on stop");
 check(d->stop()==STNLABZ_MODULE_ERR_INVALID_STATE,"duplicate stop rejected");
 printf("Expression tests: %u executed, %u failed\n",executed,failed);
 return failed?1:0;
}
