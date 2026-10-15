#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/module.h"
#include "observation.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
int main(void){
 stnlabz_module_qualification_result_t q={0};
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 if(!d||strcmp(d->id,"observation")||d->version_minor!=1||d->version_patch!=1)return 1;
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_failed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
 { digit_observation_recent_request_t request={0};
   digit_observation_recent_response_t response={0};
   request.version=DIGIT_OBSERVATION_READ_VERSION;
   strcpy(request.transport,"irc");
   strcpy(request.channel,"#stn-labz");
   request.limit=DIGIT_OBSERVATION_RECENT_MAX;
   if(strcmp(DIGIT_OBSERVATION_READ_SERVICE,"observation.recent")||
      sizeof response.items/sizeof response.items[0]!=DIGIT_OBSERVATION_RECENT_MAX)
    return 6;
 }
 puts("Observation module descriptor checks: PASS");return 0;
}
