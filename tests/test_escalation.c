#include <stdio.h>
#include <string.h>
#include "core_policy.h"
#include "logging.h"
static unsigned count,failed;
static void check(int ok,const char *msg){++count;if(!ok){++failed;fprintf(stderr,"FAIL escalation: %s\n",msg);}}
int main(int argc,char **argv){
 digit_policy_request_t p={"operator","mission","module.activate","test",1,1,1,0,"lead","fixture",1};
 digit_policy_result_t r; FILE *f;char line[2048];unsigned pending=0,unavailable=0;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_ESCALATE&&r.escalation_status==DIGIT_ESCALATION_PENDING_DELIVERY,"outside authority remains escalation");
 p.escalation_target=NULL;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY&&r.escalation_status==DIGIT_ESCALATION_UNAVAILABLE,"missing recipient cannot escalate");
 p.escalation_target="bad recipient";check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY&&r.escalation_status==DIGIT_ESCALATION_UNAVAILABLE,"invalid recipient cannot escalate");
 p.escalation_target="lead";p.authority_granted=1;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_ALLOW&&r.escalation_status==DIGIT_ESCALATION_NONE,"authorized request has no report");
 check(digit_log_sync(),"synchronized audit");digit_log_close();
 f=fopen(argv[1],"r");if(f){while(fgets(line,sizeof line,f)){if(strstr(line,"ESCALATION REPORT")&&strstr(line,"status=pending_delivery")&&strstr(line,"execution=prohibited"))++pending;if(strstr(line,"ESCALATION REPORT")&&strstr(line,"status=recipient_unavailable")&&strstr(line,"execution=prohibited"))++unavailable;}fclose(f);}
 check(pending==1,"exactly one pending report recorded");check(unavailable==2,"two unavailable reports recorded");
 printf("Core escalation tests: %u executed, %u failed\n",count,failed);return failed!=0;
}
