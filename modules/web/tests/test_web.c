#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include "../src/web_module.c"
static int checks,passed;
#define CHECK(x) do {++checks;if(x)++passed;else fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}while(0)
static int calls;
static stnlabz_module_result_t fake_router(const char *service,const void *input,
 size_t input_size,void *response,size_t capacity,size_t *used)
{
 const digit_route_request_t *request=input;
 digit_route_reply_t *reply=response;
 ++calls;
 if(strcmp(service,DIGIT_ROUTER_DISPATCH_SERVICE)||
    input_size!=sizeof *request||capacity<sizeof *reply)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(strcmp(request->sender,"digit-web")||strcmp(request->text,"Hello"))return STNLABZ_MODULE_ERR_NOT_FOUND;
 memset(reply,0,sizeof *reply);
 strcpy(reply->text,"Hello!");
 reply->replied=1;
 if(used)*used=sizeof *reply;
 return STNLABZ_MODULE_OK;
}
int main(void)
{
 stnlabz_module_qualification_result_t q={0};
 stnlabz_module_host_t mock={.invoke_service=fake_router};
 web_job_t job={0};
 char reply[DIGIT_ROUTE_REPLY_MAX]={0},payload[1400];
 CHECK(qualify(&q)==STNLABZ_MODULE_OK);
 CHECK(q.tests_executed>=STNLABZ_MODULE_MIN_TESTS);
 CHECK(q.tests_executed==q.tests_passed);
 CHECK(q.negative_test_executed&&q.negative_test_passed);
 CHECK(qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(start(NULL)==STNLABZ_MODULE_ERR_START_FAILED);
 CHECK(stop()==STNLABZ_MODULE_OK);
 CHECK(stnlabz_module_get_descriptor()->version_patch==1);
 CHECK(parse_job("{\"id\":\"test-1\",\"message\":\"Hello\"}",&job));
 module_host=&mock;
 CHECK(dispatch(&job,reply));
 CHECK(!strcmp(reply,"Hello!"));
 CHECK(calls==1);
 CHECK(build_reply(job.id,1,reply,payload,sizeof payload));
 CHECK(strstr(payload,"\"id\":\"test-1\"")!=NULL);
 CHECK(strstr(payload,"\"message\":\"Hello!\"")!=NULL);
 CHECK(!parse_job("{\"id\":\"x\",\"message\":\"hi\\u0000there\"}",&job));
 module_host=NULL;
 printf("Web HTTP transport tests %d/%d %s\n",passed,checks,
     passed==checks?"PASS":"FAIL");
 return passed==checks?0:1;
}
