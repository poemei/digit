#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "identity_model.h"
#define IDENTITY_RECORD "/opt/digit/modules/identity/identity.conf"
static identity_record_t record;
static const stnlabz_module_host_t *owner;
static int active;
static stnlabz_module_result_t controller(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 char output[1200];
 (void)unused;
 if (!cmd||!reply||!memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if (!identity_reply(&record,cmd->arguments,output,sizeof output))
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 /* Core currently bounds a single routed response. Never truncate a mission silently. */
 if (strlen(output)>=512U)
  return STNLABZ_MODULE_ERR_INVALID_STATE;
 return reply(context,output)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
 unsigned e=0,p=0;int ne=0,np=0;
 if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 if (!identity_selftest(&e,&p,&ne,&np)) return STNLABZ_MODULE_ERR_QUALIFICATION;
 q->tests_executed=e;q->tests_passed=p;q->tests_failed=e-p;
 q->negative_test_executed=ne;q->negative_test_passed=np;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
 if(!host||!host->register_command||!host->unregister_command) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!identity_load(IDENTITY_RECORD,&record)) {
  fprintf(stderr,"[IDENTITY] authoritative record unavailable or invalid\n");
  return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(!host->register_command("identity",controller,NULL)) return STNLABZ_MODULE_ERR_START_FAILED;
 owner=host;active=1;
 fprintf(stderr,"[IDENTITY] read-only mission controller active\n");
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(active&&owner && !owner->unregister_command("identity",NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 active=0;owner=NULL;memset(&record,0,sizeof record);
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "identity","Digit Identity and Mission",1,0,0,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
