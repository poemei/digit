#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "recognition.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
static stnlabz_module_service_handler_fn handler;
static int reg(const char *s,stnlabz_module_service_handler_fn fn,void *c){
 (void)c;if(strcmp(s,DIGIT_RECOGNITION_SERVICE))return 0;
 handler=fn;return 1;
}
static int unreg(const char *s,void *c){(void)c;return !strcmp(s,DIGIT_RECOGNITION_SERVICE);}
#define CHECK(expr) do{if(!(expr)){fprintf(stderr,"Recognition FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
 stnlabz_module_host_t h={0};stnlabz_module_qualification_result_t qualification={0};
 digit_recognition_request_t q={0};
 digit_recognition_response_t r={0};size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 h.register_service=reg;h.unregister_service=unreg;
 CHECK(d->version_minor==2&&d->version_patch==0);
 CHECK(d->qualify(&qualification)==STNLABZ_MODULE_OK);
 CHECK(d->start(&h)==STNLABZ_MODULE_OK&&handler);
 q.version=DIGIT_RECOGNITION_VERSION;
 strcpy(q.message,"That server crahsed agaaain!!");strcpy(q.sender,"PoeMei");
 strcpy(q.corpus_text,"A server crashed again and requires troubleshooting.");
 q.history_count=1;strcpy(q.history[0].sender,"Other_Person");strcpy(q.history[0].text,"That server crashed yesterday");
 CHECK(handler(&q,sizeof q,&r,sizeof r,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(used==sizeof r&&r.corpus_relation==DIGIT_RECOGNITION_RELATED);
 CHECK(r.conversation_relation==DIGIT_RECOGNITION_RELATED);
 CHECK(r.meaning_established==0&&r.subject_continuity==1&&r.direct_address==0);
 strcpy(q.message,"Completely different conversation");
 CHECK(handler(&q,sizeof q,&r,sizeof r,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(r.corpus_relation!=DIGIT_RECOGNITION_RELATED);
 CHECK(r.conversation_relation!=DIGIT_RECOGNITION_RELATED);
 CHECK(r.meaning_established==0&&r.subject_continuity==0);
 strcpy(q.message,"Digit, what happened?");q.addressed=1;
 CHECK(handler(&q,sizeof q,&r,sizeof r,&used,NULL)==STNLABZ_MODULE_OK);
 CHECK(r.direct_address==1&&r.meaning_established==0);
 q.history_count=DIGIT_RECOGNITION_HISTORY_MAX+1;
 CHECK(handler(&q,sizeof q,&r,sizeof r,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 q.history_count=0;q.message[0]=0;
 CHECK(handler(&q,sizeof q,&r,sizeof r,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT);
 CHECK(d->stop()==STNLABZ_MODULE_OK);
 puts("Recognition tests: PASS");return 0;
}
