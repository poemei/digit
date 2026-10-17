#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "conversation.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int reg(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(name,DIGIT_CONVERSATION_SERVICE))return 0;handler=fn;return 1;
}
static int unreg(const char *name,void *ctx){
 (void)ctx;return !strcmp(name,DIGIT_CONVERSATION_SERVICE);
}
int main(void){
 stnlabz_module_qualification_result_t q={0};
 stnlabz_module_host_t host={0};
 digit_conversation_request_t request={0};
 digit_conversation_response_t response={0};
 size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 (void)unsetenv("DIGIT_CONVERSATION_MODEL");
 host.register_service=reg;host.unregister_service=unreg;
 if(strcmp(d->id,"conversation")||d->version_minor!=1||
    d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||
    q.tests_failed||q.negative_test_passed!=2) return 1;
 if(d->start(&host)!=STNLABZ_MODULE_OK||!handler)return 2;
 if(handler(&request,sizeof request,&response,sizeof response,&used,NULL)
    !=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 3;
 request.version=DIGIT_CONVERSATION_VERSION;
 strcpy(request.sender,"Tester");strcpy(request.message,"This feels stoopid");
 if(handler(&request,sizeof request,&response,sizeof response,&used,NULL)
    !=STNLABZ_MODULE_ERR_NOT_FOUND)return 4; /* no configured local model */
 if(d->stop()!=STNLABZ_MODULE_OK)return 5;
 puts("Conversation module tests: PASS (offline; live model generation not tested)");
 return 0;
}
