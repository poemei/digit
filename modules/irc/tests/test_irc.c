#include <stdio.h>
#include <string.h>
/* Include implementation so this module test exercises the real private filter. */
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
 if(!d||strcmp(d->id,"irc")||d->version_minor!=1||d->version_patch!=3)return 1;
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
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
 if(!check_observation(":alice!user@host PRIVMSG #digit :bad\rtext",NULL,3))
  return 11;
 if(!check_observation(":alice!user@host PRIVMSG #digit :bad\ntext",NULL,3))
  return 12;
 host=NULL;
 puts("IRC descriptor, negative, and observation regression tests: PASS");
 return 0;
}
