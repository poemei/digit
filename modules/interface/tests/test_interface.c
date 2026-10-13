#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
static int checks,failed;
static void check(int ok,const char *name){
 checks++;if(!ok){failed++;fprintf(stderr,"FAIL interface: %s\n",name);}
}
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
int main(void){
 stnlabz_module_qualification_result_t result;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 check(d!=NULL,"descriptor");
 check(!strcmp(d->id,"interface"),"module identity");
 check(d->version_major==0&&d->version_minor==1&&d->version_patch==2,"version");
 check(d->required_core_api_major==STNLABZ_MODULE_API_MAJOR&&d->required_core_api_minor==STNLABZ_MODULE_API_MINOR,"ABI");
 check(d->qualify(&result)==STNLABZ_MODULE_OK,"qualification");
 check(result.tests_executed==STNLABZ_MODULE_MIN_TESTS&&result.tests_passed==STNLABZ_MODULE_MIN_TESTS,"qualification results");
 check(result.negative_test_executed==1&&result.negative_test_passed==1,"negative qualification");
 check(d->qualify(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"missing qualification storage rejected");
 check(d->stop()==STNLABZ_MODULE_ERR_INVALID_STATE,"stop before start rejected");
 check(d->start!=NULL,"listener startup callback registered");
 printf("Interface tests: %d executed, %d failed\n",checks,failed);
 return failed?1:0;
}
