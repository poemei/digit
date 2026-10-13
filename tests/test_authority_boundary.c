#include <stdio.h>
#include <string.h>
#include "core_activation.h"
#include "logging.h"
static unsigned count,failed,starts;
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){(void)host;++starts;return STNLABZ_MODULE_OK;}
static void check(int ok,const char *msg){++count;if(!ok){++failed;fprintf(stderr,"FAIL authority boundary: %s\n",msg);}}
int main(int argc,char **argv){
 stnlabz_module_registry_t reg;stnlabz_module_descriptor_t d;digit_policy_request_t p={"operator","mission","module.activate","boundary",1,1,1,1,NULL,"fixture",1};
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;stnlabz_module_registry_init(&reg);memset(&d,0,sizeof d);strcpy(d.id,"boundary");strcpy(d.name,"Boundary fixture");d.qualify=qualify;d.start=start;d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 check(stnlabz_module_registry_discover(&reg,&d)==STNLABZ_MODULE_OK,"discover");
 check(stnlabz_module_registry_verify(&reg,d.id)==STNLABZ_MODULE_OK,"verify");
 check(stnlabz_module_registry_qualify(&reg,d.id)==STNLABZ_MODULE_OK,"qualify");
 check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"no prior grant denied");
 check(digit_core_authorize(&reg,d.id,&p)==STNLABZ_MODULE_OK,"authorized verified fixture");
 p.context_verified=0;check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"unverified activation denied");
 p.context_verified=1;p.authority_granted=0;check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"revoked scope denied");
 p.authority_granted=1;p.operation="module.stop";check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"wrong operation denied");
 p.operation="module.activate";p.module_id="other";check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"wrong target denied");
 check(starts==0,"no unauthorized start callback");
 p.module_id="boundary";check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_OK,"valid activation");
 check(starts==1,"exactly one authorized callback");
 check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_QUALIFIED,"duplicate activation blocked");
 check(starts==1,"no duplicate callback");
 check(digit_core_stop_all(&reg),"stop all");
 check(digit_core_activate(&reg,d.id,NULL,&p)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"stopped requires reauthorization");
 stnlabz_module_registry_destroy(&reg);digit_log_close();
 printf("Core authority boundary tests: %u executed, %u failed\n",count,failed);return failed!=0;
}
