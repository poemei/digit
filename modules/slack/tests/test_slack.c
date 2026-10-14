#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
extern int digit_slack_format_reply(const char *,char *,size_t);
extern int digit_slack_addressed_text(const char *,const char *,const char *,const char *,const char *,char *,size_t);
int main(void){
 stnlabz_module_qualification_result_t q={0};char formatted[512];
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 if(!d||strcmp(d->id,"slack")||d->version_major!=0||d->version_minor!=1||d->version_patch!=3)return 1;
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed||!q.negative_test_passed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
 if(!digit_slack_format_reply("DSD SCOPED REPORT\\nRequest: slack-2\\nRoute: /ask\\nFinding: recorded only.",formatted,sizeof formatted)||
     strcmp(formatted,"*DSD SCOPED REPORT*\nRequest: slack-2\nRoute: /ask\nFinding: recorded only."))return 6;
 if(!digit_slack_format_reply("Hello. I'm Digit.",formatted,sizeof formatted)||
     strcmp(formatted,"Hello. I'm Digit."))return 7;
 if(digit_slack_format_reply("DSD SCOPED REPORT\\nRequest: slack-2",formatted,8))return 8;
 if(!digit_slack_format_reply("DSD SCOPED REPORT\nRequest: x",formatted,sizeof formatted)||
     strcmp(formatted,"*DSD SCOPED REPORT*\nRequest: x"))return 9;
 if(!digit_slack_addressed_text("message","C0INTEL","C0INTEL","U123","digit-bot: research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 10;
 if(!digit_slack_addressed_text("app_mention","C0INTEL","C0INTEL","U123","<@U123> research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 11;
 if(!digit_slack_addressed_text("message","C0INTEL","C0INTEL","U123","Digit research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 12;
 if(digit_slack_addressed_text("message","C0OTHER","C0INTEL","U123","digit-bot: research PHP",formatted,sizeof formatted))return 13;
 if(digit_slack_addressed_text("message","C0INTEL","C0INTEL","U123","research PHP",formatted,sizeof formatted))return 14;
 if(digit_slack_addressed_text("message","C0INTEL","C0INTEL","U123","digit-bot:",formatted,sizeof formatted))return 15;
 if(digit_slack_addressed_text("message","C0INTEL","C0INTEL","U123","digit-bot: research PHP",formatted,4))return 16;
 puts("Slack descriptor, formatting and negative tests: PASS");return 0;
}
