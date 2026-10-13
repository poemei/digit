#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module_registry.h"
static unsigned count,failed;
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;}
static void check(int ok,const char *msg){++count;if(!ok){++failed;fprintf(stderr,"FAIL lifecycle isolation: %s\n",msg);}}
int main(void){
 stnlabz_module_registry_t reg;stnlabz_module_descriptor_t d;const stnlabz_module_record_t *r;
 stnlabz_module_registry_init(&reg);memset(&d,0,sizeof d);strcpy(d.id,"isolation");strcpy(d.name,"Isolation fixture");d.qualify=qualify;d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 check(stnlabz_module_registry_discover(&reg,&d)==STNLABZ_MODULE_OK,"discover");
 check(stnlabz_module_registry_authorize_activation(&reg,d.id)==STNLABZ_MODULE_ERR_NOT_QUALIFIED,"discovered denied");
 check(stnlabz_module_registry_verify(&reg,d.id)==STNLABZ_MODULE_OK,"verify");
 check(stnlabz_module_registry_authorize_activation(&reg,d.id)==STNLABZ_MODULE_ERR_NOT_QUALIFIED,"unqualified denied");
 check(stnlabz_module_registry_qualify(&reg,d.id)==STNLABZ_MODULE_OK,"qualify");
 check(stnlabz_module_registry_activate(&reg,d.id)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"qualified alone denied");
 check(stnlabz_module_registry_authorize_activation(&reg,d.id)==STNLABZ_MODULE_OK,"explicit registry grant");
 check(stnlabz_module_registry_activate(&reg,d.id)==STNLABZ_MODULE_OK,"explicit registry activation");
 check(stnlabz_module_registry_stop(&reg,d.id)==STNLABZ_MODULE_OK,"stop");
 r=stnlabz_module_registry_find(&reg,d.id);check(r&&r->state==STNLABZ_MODULE_STATE_STOPPED&&!r->activation_authorized,"stop revokes grant");
 check(stnlabz_module_registry_activate(&reg,d.id)==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,"stopped denied without regrant");
 check(stnlabz_module_registry_quarantine(&reg,d.id)==STNLABZ_MODULE_OK,"quarantine");
 check(stnlabz_module_registry_authorize_activation(&reg,d.id)==STNLABZ_MODULE_ERR_QUARANTINED,"quarantine denies grant");
 check(stnlabz_module_registry_activate(&reg,d.id)==STNLABZ_MODULE_ERR_QUARANTINED,"quarantine denies activation");
 stnlabz_module_registry_destroy(&reg);
 printf("Core lifecycle isolation tests: %u executed, %u failed\n",count,failed);return failed!=0;
}
