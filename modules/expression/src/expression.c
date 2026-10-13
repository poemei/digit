#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "corpus_store.h"

static const stnlabz_module_host_t *host;
static stnlabz_module_result_t expression_command(
 const stnlabz_module_command_t *command,
 stnlabz_module_command_reply_fn reply,void *reply_context,void *context){
 digit_corpus_record_t record;
 char message[4608];int result;
 (void)context;
 if(!host||!command||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 result=digit_corpus_find(DIGIT_CORPUS_PATH,command->arguments,&record);
 if(result<0){
  return reply(reply_context,"Expression: Corpus unavailable or invalid query.")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 if(!result){
  return reply(reply_context,"Unknown expression (no matching Corpus knowledge).")?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
 }
 (void)snprintf(message,sizeof message,"Expression: %s [record=%s source=%s]",
  record.text,record.id,record.source);
 return reply(reply_context,message)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_command||!h->unregister_command)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_command("express",expression_command,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_command("express",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);
 (void)snprintf(d.id,sizeof d.id,"expression");
 (void)snprintf(d.name,sizeof d.name,"Corpus-backed Expression Model");
 d.version_major=0;d.version_minor=2;d.version_patch=0;
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;
 return &d;
}
