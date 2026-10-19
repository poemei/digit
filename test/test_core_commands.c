/* [AI:GPT-6 | 2026-10-19] Core operator command isolation test.
 * Exercises actual static digit.message handler without startup or IRC.
 */
#include <stdio.h>
#include <string.h>
#include "../core/digit.c"
static unsigned pos,neg,fail;
static void check(int ok,const char *label,int negative){
 if(negative)neg++;else pos++;
 printf("%s: %s\n",ok?"PASS":"FAIL",label);
 if(!ok)fail++;
}
static stnlabz_module_result_t send(const char *sender,const char *account,
 const char *target,const char *message,char *reply,size_t cap,size_t *used){
 digit_message_t m={0};
 snprintf(m.sender,sizeof m.sender,"%s",sender);
 snprintf(m.account,sizeof m.account,"%s",account);
 snprintf(m.target,sizeof m.target,"%s",target);
 snprintf(m.message,sizeof m.message,"%s",message);
 memset(reply,0,cap);
 return digit_service("digit.message",&m,sizeof m,reply,cap,used);
}
int main(void){
 stnlabz_module_registry_t registry;
 stnlabz_module_record_t fixture={0};
 char reply[512];
 size_t used=0;
 stnlabz_module_result_t rc;
 stnlabz_module_registry_init(&registry);
 registry.modules=&fixture;registry.count=1;registry.capacity=1;
 snprintf(fixture.descriptor.id,sizeof fixture.descriptor.id,"fixture");
 fixture.descriptor.version_major=1;fixture.descriptor.version_minor=2;
 fixture.descriptor.version_patch=3;
 fixture.state=STNLABZ_MODULE_STATE_ACTIVE;
 fixture.qualification.tests_executed=20;
 fixture.qualification.tests_passed=20;
 live_registry=&registry;
 pending_action=0;memset(pending_id,0,sizeof pending_id);

 used=777;
 rc=send("outsider","outsider","outsider","show modules",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"require authenticated STN_Boss DM")&&
       !pending_action,"Unauthorized account denied module inventory",1);
 used=777;
 rc=send("STN_Boss","outsider","STN_Boss","activate module fixture",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"require authenticated STN_Boss DM")&&
       !pending_action,"Forged nickname cannot authorize activation",1);
 used=777;
 rc=send("STN_Boss","STN_Boss","#stn-labz","activate module fixture",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&used==0&&!pending_action,
       "Public channel activation is silent and cannot queue changes",1);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","show modules",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"fixture v1.2.3=ACTIVE (20/20)")&&
       !pending_action,"Authenticated operator DM reads real registry inventory",0);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","activate module",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"Commands: show modules")&&
       !pending_action,"Incomplete activation syntax rejected",1);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","activate module unknown",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"Unknown module")&&
       !pending_action,"Unknown module rejected",1);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","activate module fixture",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"Activation requested")&&
       pending_action==1&&!strcmp(pending_id,"fixture"),
       "Authorized operator request queued without directly changing state",0);
 check(fixture.state==STNLABZ_MODULE_STATE_ACTIVE,
       "Queuing operator request leaves current registry state unchanged",0);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","deactivate module fixture",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"Command already pending")&&
       pending_action==1&&!strcmp(pending_id,"fixture"),
       "Competing management request cannot overwrite pending action",1);
 pending_action=0;memset(pending_id,0,sizeof pending_id);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","deactivate module fixture",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"Deactivation requested")&&
       pending_action==2&&!strcmp(pending_id,"fixture"),
       "Authorized operator deactivation request queued",0);
 pending_action=0;memset(pending_id,0,sizeof pending_id);
 used=777;
 rc=send("STN_Boss","STN_Boss","STN_Boss","show modules",reply,sizeof reply,&used);
 check(rc==STNLABZ_MODULE_OK&&strstr(reply,"fixture v1.2.3=ACTIVE")&&
       !pending_action,"Core remains operational after rejected requests",0);
 live_registry=NULL;
 printf("Core operator command: positive=%u negative=%u failures=%u\n",pos,neg,fail);
 return fail?1:0;
}
