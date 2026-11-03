#include <stdio.h>
#include <string.h>
#include "../src/web_module.c"

static int checks,passed;
#define CHECK(x) do {++checks;if(x)++passed;else fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);}while(0)
int main(void)
{
 stnlabz_module_qualification_result_t q={0};
 unsigned char header[4];
 size_t size=0;
 uid_t uid=0;
 CHECK(qualify(&q)==STNLABZ_MODULE_OK);
 CHECK(q.tests_executed>=STNLABZ_MODULE_MIN_TESTS);
 CHECK(q.tests_executed==q.tests_passed);
 CHECK(q.negative_test_executed==1&&q.negative_test_passed==1);
 CHECK(qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(!strcmp(stnlabz_module_get_descriptor()->id,"web"));
 CHECK(stnlabz_module_get_descriptor()->version_major==1);
 CHECK(stnlabz_module_get_descriptor()->version_minor==0);
 CHECK(stnlabz_module_get_descriptor()->version_patch==0);
 CHECK(start(NULL)==STNLABZ_MODULE_ERR_START_FAILED);
 CHECK(stop()==STNLABZ_MODULE_OK);
 CHECK(parse_uid("1234",&uid)&&uid==(uid_t)1234);
 CHECK(!parse_uid("1234x",&uid));
 CHECK(encode_request_length(128,header));
 CHECK(decode_request_length(header,&size)&&size==128U);
 CHECK(validate_text("/github poemei/digit tree",25));
 CHECK(!validate_text("/github\ntree",12));
 printf("Web transport: %d/%d checks passed\n",passed,checks);
 return checks==passed?0:1;
}
