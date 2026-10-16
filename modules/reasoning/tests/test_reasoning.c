#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "reasoning.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int register_service(const char *name,stnlabz_module_service_handler_fn fn,void *ctx){
 (void)ctx;if(strcmp(name,DIGIT_REASONING_SERVICE))return 0;
 handler=fn;return 1;
}
static int unregister_service(const char *name,void *ctx){
 (void)ctx;return strcmp(name,DIGIT_REASONING_SERVICE)==0;
}
static digit_reasoning_request_t *request(void){
 digit_reasoning_request_t *q=calloc(1,sizeof *q);
 if(!q)return NULL;
 q->version=DIGIT_REASONING_VERSION;
 strcpy(q->transport,"irc");strcpy(q->channel,"#stn-labz");
 strcpy(q->sender,"PoeMei");
 strcpy(q->message,"Digit observation update");
 q->observation_state=DIGIT_REASONING_PRESENT;
 q->observation_count=2;
 strcpy(q->observations[0].event_id,"irc-123");
 strcpy(q->observations[0].sender,"STN_Boss");
 strcpy(q->observations[0].text,"Testing Digit observation in the channel");
 strcpy(q->observations[1].event_id,"irc-124");
 strcpy(q->observations[1].sender,"STN_Boss");
 strcpy(q->observations[1].text,"The weather outside is pleasant");
 q->corpus_state=DIGIT_REASONING_PRESENT;
 strcpy(q->corpus_id,"C-100");strcpy(q->corpus_source,"fixture");
 strcpy(q->corpus_text,"A test record about observation");
 return q;
}
int main(void){
 stnlabz_module_host_t h={0};stnlabz_module_qualification_result_t qual={0};
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 digit_reasoning_request_t *q=request();
 digit_reasoning_response_t *out=calloc(1,sizeof *out);
 size_t n=0;
 if(!q||!out)return 1;
 h.register_service=register_service;h.unregister_service=unregister_service;
 if(!d||strcmp(d->id,"reasoning")||d->version_patch!=1||
    d->qualify(&qual)!=STNLABZ_MODULE_OK||qual.tests_executed<10||
    qual.tests_failed||!qual.negative_test_passed)return 2;
 if(d->start(&h)!=STNLABZ_MODULE_OK||!handler)return 3;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)!=STNLABZ_MODULE_OK||
    n!=sizeof *out||out->relevance!=DIGIT_REASONING_CONTEXTUAL||
    out->knowledge!=DIGIT_REASONING_CORPUS_SUPPORTED||
    out->matched_observations!=1||strcmp(out->observation_ids[0],"irc-123")||
    strcmp(out->corpus_id,"C-100"))return 4;
 q->addressed=1;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)!=STNLABZ_MODULE_OK||
    out->response_eligible!=1)return 12;
 q->addressed=0;
 strcpy(q->message,"pineapple turbines");
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)!=STNLABZ_MODULE_OK||
    out->relevance!=DIGIT_REASONING_NONE||out->matched_observations)return 5;
 q->corpus_state=DIGIT_REASONING_EMPTY;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)!=STNLABZ_MODULE_OK||
    out->knowledge!=DIGIT_REASONING_UNKNOWN||out->corpus_id[0])return 6;
 q->corpus_state=DIGIT_REASONING_UNAVAILABLE;
 q->observation_state=DIGIT_REASONING_UNAVAILABLE;q->observation_count=0;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)!=STNLABZ_MODULE_OK||
    out->knowledge!=DIGIT_REASONING_EVIDENCE_UNAVAILABLE||
    out->relevance!=DIGIT_REASONING_UNDETERMINED)return 7;
 q->observation_count=1;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)==STNLABZ_MODULE_OK)return 8;
 q->observation_state=DIGIT_REASONING_PRESENT;
 q->observations[0].event_id[0]=0;
 if(handler(q,sizeof *q,out,sizeof *out,&n,NULL)==STNLABZ_MODULE_OK)return 9;
 if(handler(NULL,sizeof *q,out,sizeof *out,&n,NULL)==STNLABZ_MODULE_OK)return 10;
 if(d->stop()!=STNLABZ_MODULE_OK)return 11;
 free(q);free(out);
 puts("Reasoning relationship tests: PASS");return 0;
}
