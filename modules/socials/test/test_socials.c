#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "socials.h"

static stnlabz_module_service_handler_fn callback;
static void *registered_context;
static int registrations;
static int reg(const char *name,stnlabz_module_service_handler_fn fn,void *ctx) {
 if(strcmp(name,DIGIT_SOCIALS_SERVICE)||!fn||callback)return 0;
 callback=fn;registered_context=ctx;registrations++;return 1;
}
static int unreg(const char *name,void *ctx) {
 if(strcmp(name,DIGIT_SOCIALS_SERVICE)||!callback||ctx!=registered_context)return 0;
 callback=NULL;return 1;
}
static void demand(int condition,const char *what) {
 if(!condition){fprintf(stderr,"FAIL: %s\n",what);exit(1);}
}
int main(int argc,char **argv) {
 void *library;
 const stnlabz_module_descriptor_t *(*factory)(void);
 const stnlabz_module_descriptor_t *d;
 stnlabz_module_qualification_result_t qualification={0};
 stnlabz_module_host_t api={0};
 const char *categories[]={"hearts","smiles","cats","paws","playful",
     "comfort","sad","celebrate","gestures","text"};
 size_t i,used;
 digit_socials_request_t request={0};
 digit_socials_response_t output={0};
 char previous[DIGIT_SOCIALS_EXPRESSION_MAX];
 if(argc!=2)return 2;
 library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
 demand(library!=NULL,"dlopen");
 *(void **)(&factory)=dlsym(library,"stnlabz_module_get_descriptor");
 demand(factory!=NULL,"factory");
 d=factory();
 demand(d&&strcmp(d->id,"socials")==0,"identity");
 demand(d->required_core_api_major==1&&d->required_core_api_minor==5,"ABI");
 demand(d->qualify(&qualification)==STNLABZ_MODULE_OK,"qualification");
 demand(qualification.tests_executed>=20&&qualification.tests_failed==0,
        "qualification checks");
 api.register_service=reg;api.unregister_service=unreg;
 demand(d->start(&api)==STNLABZ_MODULE_OK,"start");
 demand(registrations==1&&callback!=NULL,"registration");
 for(i=0;i<sizeof categories/sizeof categories[0];i++){
  unsigned trial;
  memset(&request,0,sizeof request);
  memcpy(request.category,categories[i],strlen(categories[i]));
  memset(previous,0,sizeof previous);
  for(trial=0;trial<12;trial++){
   memset(&output,0,sizeof output);used=0;
   demand(callback(&request,sizeof request,&output,sizeof output,&used,
       registered_context)==STNLABZ_MODULE_OK,"selection");
   demand(used==sizeof output && output.expression[0] &&
       memchr(output.expression,0,sizeof output.expression)!=NULL,"UTF8 response");
   demand(strcmp(previous,output.expression)!=0,"no immediate repetition");
   memcpy(previous,output.expression,sizeof previous);
  }
 }
 memset(&request,0,sizeof request);
 strcpy(request.category,"unknown");
 demand(callback(&request,sizeof request,&output,sizeof output,&used,NULL)==
        STNLABZ_MODULE_ERR_NOT_FOUND,"unknown category");
 demand(d->stop()==STNLABZ_MODULE_OK,"stop");
 demand(callback==NULL,"unregistration");
 dlclose(library);
 printf("socials qualification %u/%u; service tests PASS\n",
        qualification.tests_passed,qualification.tests_executed);
 return 0;
}
