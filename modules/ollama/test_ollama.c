/* [AI:GPT-6 | 2026-10-26 02:00:00 UTC] */
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "ollama.h"
static stnlabz_module_service_handler_fn service;
static int reg(const char *name,stnlabz_module_service_handler_fn cb,void *ctx){
 (void)ctx;if(strcmp(name,DIGIT_OLLAMA_GENERATE_SERVICE)||!cb)return 0;
 service=cb;return 1;
}
static int unreg(const char *name,void *ctx){
 (void)ctx;if(strcmp(name,DIGIT_OLLAMA_GENERATE_SERVICE))return 0;
 service=NULL;return 1;
}
int main(int argc,char **argv){
 void *library;const stnlabz_module_descriptor_t *(*get)(void);
 const stnlabz_module_descriptor_t *d;
 stnlabz_module_qualification_result_t q={0};
 stnlabz_module_host_t host={0};
 digit_ollama_result_t output={0};
 size_t used=123;
 if(argc!=2)return 2;
 library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 if(!library){fprintf(stderr,"dlopen: %s\n",dlerror());return 1;}
 *(void **)(&get)=dlsym(library,"stnlabz_module_get_descriptor");
 assert(get);d=get();
 assert(!strcmp(d->id,"ollama")&&d->version_major==0&&d->version_minor==1&&d->version_patch==2);
 assert(d->qualify(&q)==STNLABZ_MODULE_OK&&q.tests_executed>=20&&
 q.tests_passed==q.tests_executed&&q.negative_test_passed);
 host.register_service=reg;host.unregister_service=unreg;
 assert(d->start(&host)==STNLABZ_MODULE_OK&&service);
 assert(service(NULL,0,&output,sizeof output,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 assert(used==0);
 assert(d->stop()==STNLABZ_MODULE_OK);
 assert(service==NULL);
 assert(dlclose(library)==0);
 printf("Ollama ABI qualification: %u/%u PASS (offline)\n",q.tests_passed,q.tests_executed);
 return 0;
}
/* [End AI:GPT-6] */
