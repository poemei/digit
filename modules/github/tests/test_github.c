#include <stdio.h>
#include <string.h>
#include "github_module.h"

extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static unsigned registered_commands,removed_commands;
static unsigned registered_services,removed_services;
static stnlabz_module_command_handler_fn command_handler;
static stnlabz_module_service_handler_fn service_handler;
static int reject_command,reject_service;

static int add_command(const char *name,stnlabz_module_command_handler_fn cb,void *ctx)
{
    (void)ctx;
    if(strcmp(name,"github")||!cb||reject_command)return 0;
    ++registered_commands;command_handler=cb;return 1;
}
static int remove_command(const char *name,void *ctx)
{
    (void)ctx;
    if(strcmp(name,"github"))return 0;
    ++removed_commands;command_handler=NULL;return 1;
}
static int add_service(const char *name,stnlabz_module_service_handler_fn cb,void *ctx)
{
    (void)ctx;
    if(strcmp(name,DIGIT_GITHUB_REPORT_SERVICE)||!cb||reject_service)return 0;
    ++registered_services;service_handler=cb;return 1;
}
static int remove_service(const char *name,void *ctx)
{
    (void)ctx;
    if(strcmp(name,DIGIT_GITHUB_REPORT_SERVICE))return 0;
    ++removed_services;service_handler=NULL;return 1;
}
static int replied(void *ctx,const char *message)
{
    unsigned *calls=ctx;
    if(!message||!message[0])return 0;
    ++*calls;return 1;
}
int main(void)
{
    const stnlabz_module_descriptor_t *m=stnlabz_module_get_descriptor();
    stnlabz_module_qualification_result_t q={0};
    stnlabz_module_host_t host={0};
    digit_github_request_t request={{0}};
    digit_github_response_t response={0};
    stnlabz_module_command_t command={0};
    size_t used=0;
    unsigned checked=0,passed=0,calls=0;
#define CHECK(expr) do { ++checked; if(expr)++passed;else \
    fprintf(stderr,"FAIL test line %d: %s\n",__LINE__,#expr); } while(0)
    CHECK(m!=NULL);
    if(!m)return 1;
    CHECK(!strcmp(m->id,"github"));
    CHECK(m->required_core_api_major==STNLABZ_MODULE_API_MAJOR);
    CHECK(m->required_core_api_minor==STNLABZ_MODULE_API_MINOR);
    CHECK(m->version_major==1&&m->version_minor==0&&m->version_patch==8);
    CHECK(m->qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    CHECK(m->qualify(&q)==STNLABZ_MODULE_OK);
    CHECK(q.tests_executed>=STNLABZ_MODULE_MIN_TESTS);
    CHECK(q.tests_executed==q.tests_passed&&q.tests_failed==0);
    CHECK(q.negative_test_executed&&q.negative_test_passed);
    CHECK(m->start(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
    host.register_command=add_command;
    host.unregister_command=remove_command;
    host.register_service=add_service;
    host.unregister_service=remove_service;
    CHECK(m->start(&host)==STNLABZ_MODULE_OK);
    CHECK(registered_commands==1&&registered_services==1);
    CHECK(command_handler!=NULL&&service_handler!=NULL);
    snprintf(request.query,sizeof request.query,"unauthorized/repo summary");
    CHECK(service_handler(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
    CHECK(used==sizeof response&&response.status==1);
    CHECK(strstr(response.report,"not authorized")!=NULL);
    snprintf(request.query,sizeof request.query,"poemei/digit push");
    CHECK(service_handler(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK);
    CHECK(response.status==3);
    snprintf(command.arguments,sizeof command.arguments,"unauthorized/repo");
    CHECK(command_handler(&command,replied,&calls,NULL)==STNLABZ_MODULE_OK);
    CHECK(calls==1);
    CHECK(m->stop()==STNLABZ_MODULE_OK);
    CHECK(removed_commands==1&&removed_services==1);
    reject_command=1;
    CHECK(m->start(&host)==STNLABZ_MODULE_ERR_START_FAILED);
    CHECK(registered_services==2&&removed_services==2);
    reject_command=0;
    reject_service=1;
    CHECK(m->start(&host)==STNLABZ_MODULE_ERR_START_FAILED);
    CHECK(registered_services==2);
    reject_service=0;
    CHECK(m->start(&host)==STNLABZ_MODULE_OK);
    CHECK(m->stop()==STNLABZ_MODULE_OK);
    printf("GitHub read-only qualification %u/%u; negative=%d/%d; lifecycle %u/%u %s\n",
        q.tests_passed,q.tests_executed,q.negative_test_passed,
        q.negative_test_executed,passed,checked,passed==checked?"PASS":"FAIL");
    return passed==checked?0:1;
#undef CHECK
}
