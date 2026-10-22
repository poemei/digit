/* [AI:GPT-6 | 2026-10-22] Local ABI qualification harness. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "module.h"
int main(int argc,char **argv){
 void *library;
 const stnlabz_module_descriptor_t *(*descriptor_fn)(void);
 const stnlabz_module_descriptor_t *desc;
 stnlabz_module_qualification_result_t result={0};
 stnlabz_module_result_t rc;
 if(argc!=2)return 2;
 library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!library){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
 *(void **)(&descriptor_fn)=dlsym(library,"stnlabz_module_get_descriptor");
 if(!descriptor_fn){fputs("descriptor missing\n",stderr);dlclose(library);return 2;}
 desc=descriptor_fn();
 if(!desc||!desc->qualify||!desc->start||!desc->stop||
    desc->required_core_api_major!=STNLABZ_MODULE_API_MAJOR||
    desc->required_core_api_minor!=STNLABZ_MODULE_API_MINOR){
  fputs("descriptor invalid\n",stderr);dlclose(library);return 1;
 }
 rc=desc->qualify(&result);
 printf("Slack module qualification: %u/%u passed, %u failed; negative=%d/%d\n",
 result.tests_passed,result.tests_executed,result.tests_failed,
 result.negative_test_passed,result.negative_test_executed);
 dlclose(library);
 return rc==STNLABZ_MODULE_OK&&result.tests_executed>=STNLABZ_MODULE_MIN_TESTS&&
 !result.tests_failed&&result.negative_test_executed&&result.negative_test_passed?0:1;
}
