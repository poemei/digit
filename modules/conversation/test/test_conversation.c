#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module.h"
int main(int argc,char **argv){
 void *handle,*symbol;
 const stnlabz_module_descriptor_t *(*get)(void),*d;
 stnlabz_module_qualification_result_t q={0};
 if(argc!=2)return 1;
 handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"%s\n",dlerror());return 2;}
 symbol=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!symbol)return 3;
 memcpy(&get,&symbol,sizeof get);
 d=get();
 if(!d||strcmp(d->id,"conversation")||
    d->required_core_api_major!=1||d->required_core_api_minor!=5||
    !d->start||!d->stop||!d->qualify)return 4;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<20||
    q.tests_failed||q.tests_passed!=q.tests_executed||
    !q.negative_test_executed||!q.negative_test_passed){
  fprintf(stderr,"Conversation qualification %u/%u, failures=%u\n",
          q.tests_passed,q.tests_executed,q.tests_failed);
  return 5;
 }
 printf("Conversation offline qualification %u/%u PASS\n",
        q.tests_passed,q.tests_executed);
 dlclose(handle);
 return 0;
}
