#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module_loader.h"
int main(int argc,char **argv){
 void *handle,*symbol;
 stnlabz_module_get_descriptor_fn get;
 const stnlabz_module_descriptor_t *descriptor;
 stnlabz_module_qualification_result_t report={0};
 if(argc!=2)return 1;
 handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"dlopen failed: %s\n",dlerror());return 2;}
 symbol=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!symbol){dlclose(handle);return 3;}
 memcpy(&get,&symbol,sizeof get);
 descriptor=get();
 if(!descriptor||strcmp(descriptor->id,"observation")||
    descriptor->required_core_api_major!=STNLABZ_MODULE_API_MAJOR||
    descriptor->required_core_api_minor>STNLABZ_MODULE_API_MINOR||
    !descriptor->qualify||!descriptor->start||!descriptor->stop){
  dlclose(handle);return 4;
 }
 if(descriptor->qualify(&report)!=STNLABZ_MODULE_OK||
    report.tests_executed<STNLABZ_MODULE_MIN_TESTS||
    report.tests_passed!=report.tests_executed||report.tests_failed||
    !report.negative_test_executed||!report.negative_test_passed){
  fprintf(stderr,"qualification FAILED: executed=%u passed=%u failed=%u\n",
   report.tests_executed,report.tests_passed,report.tests_failed);
  dlclose(handle);return 5;
 }
 printf("Observation qualification PASS: %u/%u\n",report.tests_passed,report.tests_executed);
 dlclose(handle);return 0;
}
