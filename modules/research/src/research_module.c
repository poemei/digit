#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "research_internal.h"

#define RESEARCH_VERSION_MAJOR 1U
#define RESEARCH_VERSION_MINOR 0U
#define RESEARCH_VERSION_PATCH 0U
#define RESEARCH_ROOT "/opt/digit/research/sources"
static const stnlabz_module_host_t *owner;
static int registered;

static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
 stnlabz_module_command_reply_fn reply,void *context,void *unused)
{
 char topic[64],terms[RESEARCH_INPUT_MAX],result[RESEARCH_OUTPUT_MAX];
 (void)unused;
 if(!cmd||!reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!memchr(cmd->arguments,0,sizeof cmd->arguments))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!cmd->arguments[0]||!strcmp(cmd->arguments,"status")){
  (void)snprintf(result,sizeof result,
   "Research module active; local source-backed retrieval only. Findings unverified; human approval required.");
 }else if(!strcmp(cmd->arguments,"topics")){
  (void)snprintf(result,sizeof result,
   "Research topics: security, autonomous-systems, software-engineering, space-systems.");
 }else if(research_parse(cmd->arguments,topic,sizeof topic,terms,sizeof terms)){
  if(!research_search(RESEARCH_ROOT,topic,terms,result,sizeof result))
   return STNLABZ_MODULE_ERR_INVALID_STATE;
 }else{
  (void)snprintf(result,sizeof result,
   "Usage: research status | topics | <topic> <search phrase>");
 }
 return reply(context,result)?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *out)
{
 unsigned int tests=0,passed=0;int neg=0,negpass=0,ok;
 if(!out)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(out,0,sizeof *out);
 ok=research_selftests(&tests,&passed,&neg,&negpass);
 out->tests_executed=tests;out->tests_passed=passed;
 out->tests_failed=tests-passed;
 out->negative_test_executed=neg;
 out->negative_test_passed=negpass;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
 if(!host||!host->register_command||!host->unregister_command)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_command("research",command,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 owner=host;registered=1;
 fprintf(stderr,"[RESEARCH] active: local source-backed retrieval; no background network acquisition\n");
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
 if(registered&&owner)(void)owner->unregister_command("research",NULL);
 registered=0;owner=NULL;
 return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "research","Digit Research",RESEARCH_VERSION_MAJOR,RESEARCH_VERSION_MINOR,
 RESEARCH_VERSION_PATCH,STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
