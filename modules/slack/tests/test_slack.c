#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
extern int digit_slack_is_learn_command(const char *);
extern int digit_slack_learn_allowed(const char *,const char *,const char *);
extern const char *digit_slack_delivery_channel(const char *,const char *);
extern int digit_slack_format_reply(const char *,char *,size_t);
extern void digit_slack_hide_corpus_reference(char *);
extern const char *digit_slack_reply_thread(const char *);
extern int digit_slack_seen_observation(const char *,const char *,const char *);
extern int digit_slack_seen_message(const char *,const char *,const char *);
extern int digit_slack_is_intel_command(const char *);
extern int digit_slack_admit_message(const char *,const char *,const char *,const char *);
extern int digit_slack_addressed_text(const char *,const char *,const char *,const char *,char *,size_t);
extern int digit_slack_public_conversation(const char *,const char *);
extern int digit_slack_dispatch_reply_state(stnlabz_module_result_t,size_t,size_t,const char *);
int main(void){
 stnlabz_module_qualification_result_t q={0};char formatted[512];
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 if(!d||strcmp(d->id,"slack")||d->version_major!=0||d->version_minor!=2||d->version_patch!=9)return 1;
 {
  const char quiet[]="";
  const char response[]="answer";
  if(digit_slack_dispatch_reply_state(STNLABZ_MODULE_OK,sizeof quiet,sizeof quiet,quiet)!=0)return 62;
  if(digit_slack_dispatch_reply_state(STNLABZ_MODULE_OK,sizeof response,sizeof response,response)!=1)return 63;
  if(digit_slack_dispatch_reply_state(STNLABZ_MODULE_ERR_NOT_FOUND,sizeof response,sizeof response,response)!=-1)return 64;
  if(digit_slack_dispatch_reply_state(STNLABZ_MODULE_OK,0,sizeof response,response)!=-1)return 65;
  if(digit_slack_dispatch_reply_state(STNLABZ_MODULE_OK,sizeof response-1,sizeof response-1,response)!=-1)return 66;
 }
 if(!digit_slack_public_conversation("message","channel"))return 57;
 if(!digit_slack_public_conversation("message","group"))return 58;
 if(digit_slack_public_conversation("message","im"))return 59;
 if(digit_slack_public_conversation("app_mention","im"))return 60;
 if(digit_slack_public_conversation("reaction_added","channel"))return 61;
 if(strcmp(digit_slack_delivery_channel("[MODULE] HOTLOAD_ACTIVE id=reasoning","C0INTEL"),"C0C9LS4QA1X"))return 45;
 if(strcmp(digit_slack_delivery_channel("[MODULE] HOTLOAD_FAILED id=ask","C0INTEL"),"C0C9LS4QA1X"))return 46;
 if(strcmp(digit_slack_delivery_channel("Intel report","C0INTEL"),"C0INTEL"))return 47;
 if(strcmp(digit_slack_delivery_channel("[MODULEX] not a lifecycle event","C0INTEL"),"C0INTEL"))return 48;
 if(!digit_slack_is_learn_command("learn RULE | operator | fact"))return 49;
 if(!digit_slack_is_learn_command("LEARN RULE | operator | fact"))return 50;
 if(digit_slack_is_learn_command("learning something"))return 51;
 if(digit_slack_is_learn_command("learned something"))return 52;
 if(digit_slack_learn_allowed("C0C9LS4QA1X","U123","group"))return 53;
 if(digit_slack_learn_allowed("COTHER","U123","group"))return 54;
 {
  char public_text[256]="Answer from Corpus. [Corpus: LEARN-secret123; source: internal-source]";
  digit_slack_hide_corpus_reference(public_text);
  if(strcmp(public_text,"Answer from Corpus."))return 55;
  strcpy(public_text,"Ordinary answer without a reference.");
  digit_slack_hide_corpus_reference(public_text);
  if(strcmp(public_text,"Ordinary answer without a reference."))return 56;
 }
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
 if(!digit_slack_addressed_text("message","C0INTEL","U123","digit-bot: research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 10;
 if(!digit_slack_addressed_text("app_mention","C0INTEL","U123","<@U123> research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 11;
 if(!digit_slack_addressed_text("message","C0INTEL","U123","Digit research PHP",formatted,sizeof formatted)||strcmp(formatted,"Digit research PHP"))return 12;
 if(!digit_slack_addressed_text("message","C0OTHER","U123","digit-bot: research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 13;
 if(digit_slack_addressed_text("message","C0INTEL","U123","research PHP",formatted,sizeof formatted))return 14;
 if(digit_slack_addressed_text("message","C0INTEL","U123","digit-bot:",formatted,sizeof formatted))return 15;
 if(digit_slack_addressed_text("message","C0INTEL","U123","digit-bot: research PHP",formatted,4))return 16;
 if(!digit_slack_addressed_text("app_mention","C0DEV","U123","<@U123> research PHP",formatted,sizeof formatted)||strcmp(formatted,"research PHP"))return 17;
 if(digit_slack_addressed_text("message","","U123","digit-bot: research PHP",formatted,sizeof formatted))return 18;
 /* Natural addressing must not depend on name position. */
 if(!digit_slack_addressed_text("message","C0INTEL","U123",
    "so I think digit has her irc shit figured out?",formatted,sizeof formatted)||
    strcmp(formatted,"so I think digit has her irc shit figured out?"))return 38;
 if(!digit_slack_addressed_text("message","C0INTEL","U123",
    "maybe not yur slack anymore digit?",formatted,sizeof formatted)||
    strcmp(formatted,"maybe not yur slack anymore digit?"))return 39;
 if(!digit_slack_addressed_text("message","C0INTEL","U123",
    "umm digit?",formatted,sizeof formatted)||strcmp(formatted,"umm digit?"))return 40;
 if(!digit_slack_addressed_text("message","C0INTEL","U123",
    "hey <@U123> can you hear me?",formatted,sizeof formatted)||
    strcmp(formatted,"hey <@U123> can you hear me?"))return 41;
 if(digit_slack_addressed_text("message","C0INTEL","U123",
    "digital photos are cool",formatted,sizeof formatted))return 42;
 if(digit_slack_addressed_text("message","C0INTEL","U123",
    "we need digitization",formatted,sizeof formatted))return 43;
 if(!digit_slack_addressed_text("message","C0INTEL","U123",
    "digit-bot?",formatted,sizeof formatted)||strcmp(formatted,"digit-bot?"))return 44;
 if(digit_slack_reply_thread(NULL)!=NULL)return 19;
 if(digit_slack_reply_thread("")!=NULL)return 20;
 if(!digit_slack_reply_thread("1728920000.000001")||strcmp(digit_slack_reply_thread("1728920000.000001"),"1728920000.000001"))return 21;
 if(digit_slack_seen_message("T1","C1","123.45"))return 22;
 if(!digit_slack_seen_message("T1","C1","123.45"))return 23;
 if(digit_slack_seen_message("T1","C1","123.46"))return 24;
 if(digit_slack_seen_message("T1","C2","123.45"))return 25;
 /* app_mention and message deliveries with distinct event IDs but the
     same underlying Slack message timestamp must dispatch only once. */
 if(!digit_slack_admit_message("EvA","T2","C3","1800.0001"))return 26;
 if(digit_slack_admit_message("EvB","T2","C3","1800.0001"))return 27;
 if(digit_slack_admit_message("EvA","T2","C3","1800.0001"))return 28;
 if(!digit_slack_admit_message("EvC","T2","C3","1800.0002"))return 29;
 if(!digit_slack_admit_message("EvD","T2","C3",NULL))return 30;
 if(digit_slack_admit_message("EvD","T2","C3",NULL))return 32;
 if(!digit_slack_admit_message("EvE","T2","C4","1800.0001"))return 31;
 if(!digit_slack_is_intel_command("report intel")||!digit_slack_is_intel_command("REPORT INTEL")||!digit_slack_is_intel_command("intel report"))return 33;
 if(digit_slack_is_intel_command("report intel now")||digit_slack_is_intel_command("report")||digit_slack_is_intel_command(NULL))return 34;
 if(digit_slack_seen_observation("T1","C1","1.0001"))return 35;
 if(!digit_slack_seen_observation("T1","C1","1.0001"))return 36;
 if(digit_slack_seen_observation("T1","C2","1.0001"))return 37;
 puts("Slack routing, observation deduplication and Intel command recognition: PASS");return 0;
}
