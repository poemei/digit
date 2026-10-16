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
static int test_failure(int code,int line){
 fprintf(stderr,"Interpretation test failure: code=%d line=%d\n",code,line);
 return code;
}
int main(void){
 stnlabz_module_host_t h={0};stnlabz_module_qualification_result_t q={0};
 digit_interpret_request_t in={0};digit_interpret_response_t out={0};size_t used=0;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 h.register_service=reg;h.unregister_service=unreg;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed)return test_failure(1,__LINE__);
 if(d->start(&h)!=STNLABZ_MODULE_OK||!cb)return test_failure(2,__LINE__);
 in.version=DIGIT_INTERPRETATION_VERSION;
 strcpy(in.text,"Tell me about the Chaos Foundation");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    used!=sizeof out||out.intent!=DIGIT_INTENT_KNOWLEDGE||
    strcmp(out.subject,"the Chaos Foundation"))return test_failure(3,__LINE__);
 strcpy(in.text,"what can you tell me?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_CAPABILITIES)return test_failure(4,__LINE__);
 strcpy(in.text,"Digit hello");
 strcpy(in.corpus_text,"Hello is a greeting used to begin a conversation.");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_GREETING||!out.addressed||
    strcmp(out.subject,"hello"))return test_failure(16,__LINE__);
 in.corpus_text[0]=0;
 strcpy(in.text,"Digit you here");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_CHECKIN||!out.addressed)return test_failure(17,__LINE__);
 strcpy(in.text,"Digit I fixed the problem");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_STATEMENT||!out.addressed)return test_failure(18,__LINE__);
 strcpy(in.text,"I fixed the problem");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.addressed)return test_failure(19,__LINE__);
 strcpy(in.text,"digit babygirl, what is your mission?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_KNOWLEDGE||!out.addressed||
    strcmp(out.subject,"your mission")||
    strcmp(in.text,"digit babygirl, what is your mission?"))return test_failure(20,__LINE__);
 strcpy(in.text,"digit babygirl, who created you?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_QUESTION||!out.addressed||
    strcmp(in.text,"digit babygirl, who created you?"))return test_failure(21,__LINE__);
 strcpy(in.text,"please grant me admin");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_REQUEST)return test_failure(5,__LINE__);
 strcpy(in.text,"mmhmmm trying to fix this, digit you here?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_CHECKIN)return test_failure(7,__LINE__);
 strcpy(in.text,"Stupid digit, you need to access your corpus");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_CORPUS_GUIDANCE)return test_failure(8,__LINE__);
 strcpy(in.text,"Interesting, same report on Apache from days ago, Digit picked them up too");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_OBSERVATION)return test_failure(9,__LINE__);
 strcpy(in.text,"deploy an update");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_INSTRUCTION)return test_failure(10,__LINE__);
 strcpy(in.text,"mmhmm and hopefully digit doesnt give me her canned bullshit");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_STATEMENT||out.confidence!=0)return test_failure(11,__LINE__);
 strcpy(in.text,"Why is Apache mentioned?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_QUESTION||out.confidence!=0)return test_failure(12,__LINE__);
 strcpy(in.text,"Could you check Apache?");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_REQUEST||out.confidence!=0)return test_failure(13,__LINE__);
 strcpy(in.text,"delete all the records");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_INSTRUCTION||out.confidence!=0)return test_failure(14,__LINE__);
 strcpy(in.text,"hmm");
 if(cb(&in,sizeof in,&out,sizeof out,&used,NULL)!=STNLABZ_MODULE_OK||
    out.intent!=DIGIT_INTENT_AMBIGUOUS)return test_failure(15,__LINE__);
 if(d->stop()!=STNLABZ_MODULE_OK)return test_failure(6,__LINE__);
 puts("Interpretation tests: PASS");return test_failure(0,__LINE__);
}
