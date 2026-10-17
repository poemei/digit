/* The implementation defines POSIX feature macros before system headers. */
#include "../src/irc.c"

static unsigned observed;
static digit_observation_t last_observation;

static stnlabz_module_result_t capture_observation(const char *name,
 const void *request,size_t request_size,void *response,size_t response_size,
 size_t *response_used){
 const digit_observation_t *event=request;
 int saved=1;
 if(strcmp(name,DIGIT_OBSERVATION_SERVICE)||request_size!=sizeof *event||
    response_size<sizeof saved||!response||!response_used)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 last_observation=*event;
 observed++;
 memcpy(response,&saved,sizeof saved);
 *response_used=sizeof saved;
 return STNLABZ_MODULE_OK;
}
static int check_observation(const char *line,const char *expected,
 unsigned expected_count){
 observe_irc(line);
 return observed==expected_count&&
  (!expected||!strcmp(last_observation.text,expected));
}
int main(void){
 stnlabz_module_qualification_result_t q={0};
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t mock_host={0};
 if(!d||strcmp(d->id,"irc")||d->version_minor!=2||d->version_patch!=6)return 1;
 if(!digit_irc_public_conversation("#stn-labz","#stn-labz"))return 37;
 if(digit_irc_public_conversation("#other","#stn-labz"))return 38;
 if(digit_irc_public_conversation("digit-bot","#stn-labz"))return 39;
 if(digit_irc_public_conversation("#stn-labz","digit-bot"))return 40;
 {
  char answer[32]="Grounded summary.";
  if(digit_irc_dispatch_state(STNLABZ_MODULE_OK,strlen(answer)+1,
     sizeof answer,answer)!=1)return 46;
  answer[0]=0;
  if(digit_irc_dispatch_state(STNLABZ_MODULE_OK,1,
     sizeof answer,answer)!=0)return 47;
  if(digit_irc_dispatch_state(STNLABZ_MODULE_ERR_NOT_FOUND,0,
     sizeof answer,answer)!=-1)return 48;
  if(digit_irc_dispatch_state(STNLABZ_MODULE_OK,0,
     sizeof answer,answer)!=-1)return 49;
  memset(answer,'x',sizeof answer);
  if(digit_irc_dispatch_state(STNLABZ_MODULE_OK,sizeof answer,
     sizeof answer,answer)!=-1)return 50;
 }
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
 {
  char public_text[256]="Answer from Corpus. [Corpus: LEARN-secret123; source: internal-source]";
  digit_irc_hide_corpus_reference(public_text);
  if(strcmp(public_text,"Answer from Corpus."))return 35;
  strcpy(public_text,"Ordinary answer without a reference.");
  digit_irc_hide_corpus_reference(public_text);
  if(strcmp(public_text,"Ordinary answer without a reference."))return 36;
 }
 mock_host.invoke_service=capture_observation;
 host=&mock_host;
 snprintf(config.nick,sizeof config.nick,"digit-bot");
 snprintf(config.channel,sizeof config.channel,"#digit");
 if(!check_observation(":alice!user@host PRIVMSG #digit :hello","hello",1))return 6;
 if(!check_observation(":alice!user@host PRIVMSG #digit :morning everyone",
   "morning everyone",2))return 7;
 if(!check_observation(":bob!user@host PRIVMSG #digit :another conversation",
   "another conversation",3))return 8;
 if(!check_observation(":alice!user@host PRIVMSG #other :morning everyone",NULL,3))
  return 9;
 if(!check_observation(":digit-bot!user@host PRIVMSG #digit :morning everyone",
   NULL,3))return 10;
 if(!check_observation(":alice!user@host PRIVMSG #digit :",NULL,3))
  return 11;
 {
  char overlong[640];
  const char *prefix=":alice!user@host PRIVMSG #digit :";
  size_t p=strlen(prefix);
  memcpy(overlong,prefix,p);
  memset(overlong+p,'x',513);
  overlong[p+513]=0;
  if(!check_observation(overlong,NULL,3))return 12;
 }
 /* Public-channel mentions must persist even when not addressed as commands. */
 snprintf(config.channel,sizeof config.channel,"#stn-labz");
 if(!check_observation(":PoeMei!user@host PRIVMSG #stn-labz :digit-bot?",
   "digit-bot?",4))return 13;
 if(!check_observation(":PoeMei!user@host PRIVMSG #stn-labz :digit, can you tell me what I just said?",
   "digit, can you tell me what I just said?",5))return 14;
 if(!check_observation(":PoeMei!user@host PRIVMSG #stn-labz :I do wonder if digit-bot even sees this?",
   "I do wonder if digit-bot even sees this?",6))return 15;
 /* Slack-equivalent conversational admission, independent of observation. */
 {
  char text[513];
  if(!addressed_irc_text("digit-bot: what time is it?",config.nick,text,sizeof text)||
     strcmp(text,"digit-bot: what time is it?"))return 18;
  if(!addressed_irc_text("digit, can you tell me what I just said?",config.nick,text,sizeof text)||
     strcmp(text,"digit, can you tell me what I just said?"))return 19;
  if(!addressed_irc_text("digit-bot?",config.nick,text,sizeof text)||
     strcmp(text,"digit-bot?"))return 20;
  if(!addressed_irc_text("DiGiT how are you",config.nick,text,sizeof text)||
     strcmp(text,"DiGiT how are you"))return 21;
  if(addressed_irc_text("good morning everyone",config.nick,text,sizeof text))return 22;
  if(addressed_irc_text("digital systems",config.nick,text,sizeof text))return 23;
  if(addressed_irc_text("digit-bot:",config.nick,text,sizeof text))return 24;
  if(!addressed_irc_text("umm digit?",config.nick,text,sizeof text)||
     strcmp(text,"umm digit?"))return 25;
  if(!addressed_irc_text("so digit, is yur shit working now?",config.nick,text,sizeof text)||
     strcmp(text,"so digit, is yur shit working now?"))return 26;
  if(!addressed_irc_text("is digit-bot working?",config.nick,text,sizeof text)||
     strcmp(text,"is digit-bot working?"))return 27;
  if(!addressed_irc_text("what do you think, DiGiT?",config.nick,text,sizeof text)||
     strcmp(text,"what do you think, DiGiT?"))return 28;
  if(addressed_irc_text("my digital camera works",config.nick,text,sizeof text))return 29;
  if(addressed_irc_text("digitization is useful",config.nick,text,sizeof text))return 30;
  if(!addressed_irc_text("digit-bot: what is your mission?",config.nick,text,sizeof text)||
     strcmp(text,"digit-bot: what is your mission?"))return 34;
 }
 /* Notification queue admission must hold multiple reports without replacing
    the first and reject overflow without claiming delivery. */
 {
  digit_intel_delivery_t notice={0};int accepted=0;size_t used=0;
  unsigned k;char expected[64];
  notice.version=DIGIT_INTEL_DELIVERY_VERSION;
  active=1;intel_head=0;intel_count=0;
  for(k=0;k<IRC_INTEL_QUEUE_CAPACITY;k++){
   snprintf(notice.message,sizeof notice.message,"module-%u",k);
   if(publish_intel(&notice,sizeof notice,&accepted,sizeof accepted,&used,NULL)!=STNLABZ_MODULE_OK||
      !accepted||intel_count!=k+1)return 31;
  }
  snprintf(notice.message,sizeof notice.message,"overflow");
  if(publish_intel(&notice,sizeof notice,&accepted,sizeof accepted,&used,NULL)==STNLABZ_MODULE_OK||
     accepted||intel_count!=IRC_INTEL_QUEUE_CAPACITY)return 32;
  for(k=0;k<IRC_INTEL_QUEUE_CAPACITY;k++){
   snprintf(expected,sizeof expected,"module-%u",k);
   if(strcmp(intel_pending[(intel_head+k)%IRC_INTEL_QUEUE_CAPACITY],expected))return 33;
  }
  active=0;intel_head=0;intel_count=0;
 }
 /* Generic ABI module status is admitted independently of legacy Intel.
    Reject invalid versions and messages too long for an IRC wire frame. */
 {
  stnlabz_module_notification_t event={0};
  int accepted=0;size_t used=0;
  active=1;intel_head=0;intel_count=0;
  event.version=STNLABZ_NOTIFICATION_VERSION;
  strcpy(event.message,"[MODULE] HOTLOAD_ACTIVE id=interpretation active=0.1.8");
  if(publish_status(&event,sizeof event,&accepted,sizeof accepted,&used,NULL)!=STNLABZ_MODULE_OK||
     !accepted||used!=sizeof accepted||intel_count!=1||
     strcmp(intel_pending[0],event.message))return 51;
  event.version=99;
  if(publish_status(&event,sizeof event,&accepted,sizeof accepted,&used,NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT||
     intel_count!=1)return 52;
  event.version=STNLABZ_NOTIFICATION_VERSION;
  memset(event.message,'x',480);event.message[480]=0;
  if(publish_status(&event,sizeof event,&accepted,sizeof accepted,&used,NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT||
     intel_count!=1)return 53;
  active=0;intel_head=0;intel_count=0;
 }
 /* SASL and registration may complete in either order, exactly one JOIN. */
 if(irc_join_ready(0,1,0)||irc_join_ready(1,0,0)||
    !irc_join_ready(1,1,0)||irc_join_ready(1,1,1))return 16;
 if(!irc_join_confirmed(":digit-bot!user@host JOIN :#stn-labz","digit-bot","#stn-labz"))return 41;
 if(!irc_join_confirmed(":DIGIT-BOT!user@host JOIN #stn-labz","digit-bot","#stn-labz"))return 42;
 if(irc_join_confirmed(":alice!user@host JOIN :#stn-labz","digit-bot","#stn-labz"))return 43;
 if(irc_join_confirmed(":digit-bot!user@host JOIN :#other","digit-bot","#stn-labz"))return 44;
 if(irc_join_confirmed(":digit-bot!user@host PRIVMSG #stn-labz :hello","digit-bot","#stn-labz"))return 45;
 /* An SSL-buffered IRC frame must be read even if select would time out. */
 if(!irc_tls_read_ready(0,1)||!irc_tls_read_ready(1,0)||
    irc_tls_read_ready(0,0))return 17;
 host=NULL;
 puts("IRC descriptor, observation, addressed MVC admission, SASL/JOIN ordering, TLS-read readiness: PASS");
 return 0;
}
