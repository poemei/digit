#include <stdio.h>
#include <dlfcn.h>
#include "module.h"
int main(int argc,char **argv){
 void *h;const stnlabz_module_descriptor_t *(*get)(void);
 const stnlabz_module_descriptor_t *d;stnlabz_module_qualification_result_t r={0};
 stnlabz_module_result_t result;
 if(argc!=2)return 2;
 h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!h){fprintf(stderr,"dlopen failed: %s\n",dlerror());return 2;}
 *(void **)(&get)=dlsym(h,"stnlabz_module_get_descriptor");
 if(!get)return 2;
 d=get();if(!d||!d->qualify||!d->start||!d->stop||
  d->version_major!=0||d->version_minor!=5||d->version_patch!=0)return 2;
 result=d->qualify(&r);
 printf("Publisher qualification: %u/%u passed, failed=%u, negative=%d/%d\n",
 r.tests_passed,r.tests_executed,r.tests_failed,r.negative_test_passed,r.negative_test_executed);
 dlclose(h);
 return result==STNLABZ_MODULE_OK&&r.tests_executed>=STNLABZ_MODULE_MIN_TESTS&&
  !r.tests_failed&&r.negative_test_passed?0:1;
}
