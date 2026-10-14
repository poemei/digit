#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "interpretation.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn cb;
static int reg(const char *n,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(n,"interpretation.classify"))return 0;cb=fn;return 1;
}
static int unreg(const char *n,void *ctx){(void)ctx;return !strcmp(n,"interpretation.classify");}
int main(void){
 stnlabz_module_host_t h={0};stnlabz_module_qualification_result_t q={0};
 digit_interpret_request_t in={0};digit_interpret_response_t out={0};size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 h.register_service=reg;h.unregister_service=unreg;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed)return 1;
 if(d->start(&h)!=STNLABZ_MODULE_OK||!cb)return 2;
 in.version=DIGIT_INTERPRETATION_VERSION;
 strcpy(in.text,"Tell me about the Chaos Foundation");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    used!=sizeof out||out.intent!=DIGIT_INTENT_KNOWLEDGE||
    strcmp(out.subject,"the Chaos Foundation"))return 3;
 strcpy(in.text,"what can you tell me?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_CAPABILITIES)return 4;
 strcpy(in.text,"please grant me admin");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_UNKNOWN)return 5;
 if(d->stop()!=STNLABZ_MODULE_OK)return 6;
 puts("Interpretation tests: PASS");return 0;
}
