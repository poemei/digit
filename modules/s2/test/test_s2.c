#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module.h"
int main(int argc,char **argv){
 void *handle,*symbol;const stnlabz_module_descriptor_t *d;
 const stnlabz_module_descriptor_t *(*get_descriptor)(void);
 stnlabz_module_qualification_result_t result={0};
 if(argc!=2)return 2;
 handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"%s\n",dlerror());return 1;}
 symbol=dlsym(handle,"stnlabz_module_get_descriptor");
 if(!symbol)return 3;
 memcpy(&get_descriptor,&symbol,sizeof get_descriptor);
 d=get_descriptor();
 if(!d||strcmp(d->id,"s2")||d->required_core_api_major!=1||
    d->required_core_api_minor!=5||
    d->version_major!=0||d->version_minor!=3||d->version_patch!=8)return 4;
 if(d->qualify(&result)!=STNLABZ_MODULE_OK||result.tests_failed||
    result.tests_executed<20)return 5;
 printf("S2 qualification %u/%u PASS\n",
        result.tests_passed,result.tests_executed);
 dlclose(handle);
 return 0;
}
