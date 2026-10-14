#include <stdio.h>
#include <string.h>
#include "intel.h"
int main(void){
 stnlabz_module_qualification_result_t q={0};
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 if(!d||strcmp(d->id,"intel")||d->version_major!=0||d->version_minor!=1||d->version_patch!=1)return 1;
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed||!q.negative_test_passed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
 puts("Intel descriptor, admission rules and negative checks: PASS");return 0;
}
