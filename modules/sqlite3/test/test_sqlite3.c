/* SQLite3 module descriptor and real qualification-report integration test. */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "module_loader.h"
int main(int argc,char **argv) {
    void *handle,*sym;
    stnlabz_module_get_descriptor_fn get;
    const stnlabz_module_descriptor_t *d;
    stnlabz_module_qualification_result_t r={0};
    if(argc!=2)return 1;
    handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!handle){fprintf(stderr,"dlopen: %s\n",dlerror());return 2;}
    sym=dlsym(handle,"stnlabz_module_get_descriptor");
    if(!sym){dlclose(handle);return 3;}
    memcpy(&get,&sym,sizeof get);
    d=get();
    if(!d||strcmp(d->id,"sqlite3")||
       d->version_major!=1||d->version_minor!=5||d->version_patch!=4||
       d->required_core_api_major!=STNLABZ_MODULE_API_MAJOR||
       d->required_core_api_minor>STNLABZ_MODULE_API_MINOR||
       !d->qualify||!d->start||!d->stop){dlclose(handle);return 4;}
    if(d->qualify(&r)!=STNLABZ_MODULE_OK||
       r.tests_executed<STNLABZ_MODULE_MIN_TESTS||
       r.tests_passed!=r.tests_executed||r.tests_failed||
       !r.negative_test_executed||!r.negative_test_passed){
        fprintf(stderr,"qualification failed: executed=%u passed=%u failed=%u\n",
                r.tests_executed,r.tests_passed,r.tests_failed);
        dlclose(handle);return 5;
    }
    printf("SQLite3 qualification PASS: %u/%u (negative tests included)\n",
           r.tests_passed,r.tests_executed);
    dlclose(handle);
    return 0;
}
