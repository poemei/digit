#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "identity_internal.h"
#define IDENTITY_RECORD "/opt/digit/modules/identity/identity.conf"
static identity_record_t record;
static const stnlabz_module_host_t *owner;
static int command_active,service_active;

static int get_order(unsigned index,char *out,size_t cap)
{
 digit_root_order_request_t request={index};
 digit_root_order_response_t response={0};
 size_t used=0;
 if (!owner||!owner->invoke_service||!out) return 0;
 if (owner->invoke_service(DIGIT_ROOT_ORDERS_SERVICE,
         &request,sizeof request,&response,sizeof response,&used)
     != STNLABZ_MODULE_OK || used!=sizeof response ||
     !memchr(response.text,0,sizeof response.text)) return 0;
 if (strlen(response.text)+1U>cap) return 0;
 memcpy(out,response.text,strlen(response.text)+1U);
 return 1;
}
static int answer(const char *topic,char *output,size_t cap)
{
 unsigned index=0;
 char order[256];
 int n;
 if (!topic||!output||cap<2U) return 0;
 if (!strcmp(topic,"orders")||!strcmp(topic,"general orders")) {
  char a[256],b[256],c[256];
  if (!get_order(1U,a,sizeof a)||!get_order(2U,b,sizeof b)||
      !get_order(3U,c,sizeof c)) return 0;
  n=snprintf(output,cap,"My General Orders: 1. %s 2. %s 3. %s",a,b,c);
  return n>0&&(size_t)n<cap;
 }
 if (!strcmp(topic,"1")||!strcmp(topic,"first general order")) index=1U;
 else if (!strcmp(topic,"2")||!strcmp(topic,"second general order")) index=2U;
 else if (!strcmp(topic,"3")||!strcmp(topic,"third general order")) index=3U;
 if (index) {
  if (!get_order(index,order,sizeof order)) return 0;
  n=snprintf(output,cap,"My %s General Order is: %s",
    index==1U?"First":index==2U?"Second":"Third",order);
  return n>0&&(size_t)n<cap;
 }
 return identity_reply(&record,topic,output,cap);
}
static stnlabz_module_result_t controller(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 char output[DIGIT_IDENTITY_ANSWER_MAX];
 (void)unused;
 if (!cmd||!reply||!memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if (!answer(cmd->arguments,output,sizeof output))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 return reply(context,output)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t answer_service(const void *request,size_t request_size,
 void *response,size_t response_size,size_t *used,void *ctx)
{
 const digit_identity_question_t *in=request;
 digit_identity_answer_t *out=response;
 char topic[80];
 (void)ctx;
 if(used)*used=0;
 if(!in||request_size!=sizeof *in||!out||response_size<sizeof *out||
    !memchr(in->question,0,sizeof in->question))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if (!identity_question_route(in->question,topic,sizeof topic))
  return STNLABZ_MODULE_ERR_NOT_FOUND;
 memset(out,0,sizeof *out);
 if(!answer(topic,out->answer,sizeof out->answer))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(used)*used=sizeof *out;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned e=0,p=0;int ne=0,np=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 if(!identity_selftest(&e,&p,&ne,&np))return STNLABZ_MODULE_ERR_QUALIFICATION;
 q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
 q->negative_test_executed=ne;q->negative_test_passed=np;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
 if(!host||!host->register_command||!host->unregister_command||
    !host->register_service||!host->unregister_service||!host->invoke_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!identity_load(IDENTITY_RECORD,&record)) {
  fprintf(stderr,"[IDENTITY] approved record unavailable or invalid\n");
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 owner=host;
 if(!host->register_command("identity",controller,NULL))goto fail;
 command_active=1;
 if(!host->register_service(DIGIT_IDENTITY_ANSWER_SERVICE,answer_service,NULL))goto fail;
 service_active=1;
 fprintf(stderr,"[IDENTITY] mission and Core Root orders available read-only\n");
 return STNLABZ_MODULE_OK;
fail:
 if(command_active)(void)host->unregister_command("identity",NULL);
 command_active=0;owner=NULL;memset(&record,0,sizeof record);
 return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void)
{
 if(service_active&&owner&&!owner->unregister_service(DIGIT_IDENTITY_ANSWER_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 service_active=0;
 if(command_active&&owner&&!owner->unregister_command("identity",NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 command_active=0;owner=NULL;memset(&record,0,sizeof record);
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "identity","Digit Identity and Mission",1,1,0,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
