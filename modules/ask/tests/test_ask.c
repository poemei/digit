#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int identity_calls;
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t size,void *reply,size_t cap,size_t *used){
 const char *msg="I am Digit, the STN-LABZ foundational autonomous agent.";
 if(strcmp(name,"core.identity.read")||size!=1||*(const char *)req!=0||strlen(msg)+1>cap)return STNLABZ_MODULE_ERR_NOT_FOUND;
 identity_calls++;memcpy(reply,msg,strlen(msg)+1);*used=strlen(msg)+1;return STNLABZ_MODULE_OK;
}
static int reg(const char *s,stnlabz_module_service_handler_fn fn,void *ctx){(void)ctx;if(strcmp(s,"route./ask"))return 0;handler=fn;return 1;}
static int unreg(const char *s,void *ctx){(void)ctx;return strcmp(s,"route./ask")==0;}
int main(void){
 stnlabz_module_qualification_result_t q;char out[512];size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t h={0};h.register_service=reg;h.unregister_service=unreg;h.invoke_service=invoke;
 if(strcmp(d->id,"ask")||d->qualify(&q)!=STNLABZ_MODULE_OK||d->start(&h)!=STNLABZ_MODULE_OK||!handler)return 1;
 if(handler("hello",6,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"Hello"))return 1;
 if(handler("Hi digit",9,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"Hello"))return 1;
 if(d->version_patch!=2)return 1;
 if(handler("who are you?",13,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"foundational autonomous agent")||identity_calls!=1)return 1;
 if(handler("diagnose my last request",25,out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||!strstr(out,"authorized console")||identity_calls!=1)return 1;
 if(handler("",1,out,sizeof out,&used,NULL)==STNLABZ_MODULE_OK)return 1;
 if(handler("hello",6,out,2,&used,NULL)==STNLABZ_MODULE_OK)return 1;
 if(d->stop()!=STNLABZ_MODULE_OK)return 1;
 puts("Ask tests: PASS");return 0;
}
