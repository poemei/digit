#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "socials.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int fail_register;
static int register_service(const char *name, stnlabz_module_service_handler_fn fn, void *ctx) {
 (void)ctx;
 if(fail_register)return 0;
 if(strcmp(name,DIGIT_SOCIALS_SERVICE))return 0;
 handler=fn;return 1;
}
static int unregister_service(const char *name,void *ctx) {
 (void)ctx;
 if(strcmp(name,DIGIT_SOCIALS_SERVICE))return 0;
 handler=NULL;return 1;
}
int main(void) {
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t h={0};
 digit_socials_request_t q={DIGIT_SOCIALS_VERSION,DIGIT_SOCIALS_HEART,"affectionate greeting"};
 digit_socials_response_t a,b;
 stnlabz_module_qualification_result_t qual;
 size_t used=0; int checks=0;
 h.register_service=register_service;h.unregister_service=unregister_service;
 assert(!strcmp(d->id,"socials"));checks++;
 assert(d->version_major==0&&d->version_minor==1&&d->version_patch==0);checks++;
 assert(d->qualify(&qual)==STNLABZ_MODULE_OK&&qual.tests_executed>=10&&qual.negative_test_passed);checks++;
 assert(d->start(NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 fail_register=1;
 assert(d->start(&h)==STNLABZ_MODULE_ERR_START_FAILED);checks++;
 fail_register=0;
 assert(d->start(&h)==STNLABZ_MODULE_OK&&handler);checks++;
 assert(d->start(&h)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_OK&&used==sizeof a);checks++;
 assert(a.version==DIGIT_SOCIALS_VERSION&&a.category==DIGIT_SOCIALS_HEART&&a.emoji[0]);checks++;
 assert(handler(&q,sizeof q,&b,sizeof b,&used,NULL)==STNLABZ_MODULE_OK&&!strcmp(a.emoji,b.emoji));checks++;
 q.category=DIGIT_SOCIALS_ANY;
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_OK&&a.category>=1&&a.category<=5);checks++;
 q.category=DIGIT_SOCIALS_CELEBRATE+1;
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT&&used==0);checks++;
 q.category=DIGIT_SOCIALS_SMILE;q.context[0]=0;
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 memset(q.context,'X',sizeof q.context);
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 memset(q.context,0,sizeof q.context);strcpy(q.context,"celebration");
 assert(handler(&q,sizeof q,&a,sizeof a-1,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 q.version=2;
 assert(handler(&q,sizeof q,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 q.version=DIGIT_SOCIALS_VERSION;
 assert(handler(&q,sizeof q-1,&a,sizeof a,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);checks++;
 assert(d->stop()==STNLABZ_MODULE_OK&&!handler);checks++;
 assert(d->stop()==STNLABZ_MODULE_ERR_INVALID_STATE);checks++;
 printf("socials tests: %d assertions passed\n",checks);
 return 0;
}
