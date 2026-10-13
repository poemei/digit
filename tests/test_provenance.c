#include <stdio.h>
#include "core_policy.h"
#include "logging.h"
static unsigned count,failed;
static void check(int ok,const char *msg){++count;if(!ok){++failed;fprintf(stderr,"FAIL provenance: %s\n",msg);}}
int main(int argc,char **argv){
 digit_policy_request_t p={"operator","mission","module.activate","test",1,1,1,1,NULL,"fixture",1};
 digit_policy_result_t r;
 if(argc!=2||!digit_log_open_path(argv[1]))return 2;
 check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_ALLOW,"verified fixture allows");
 p.context_verified=0;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"unverified context denied");
 p.context_verified=2;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"noncanonical verification denied");
 p.context_verified=1;p.verification_source=NULL;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"absent provenance denied");
 p.verification_source="";check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"empty provenance denied");
 p.verification_source="fixture";p.actor=NULL;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"missing actor denied");
 p.actor="operator";p.mission=NULL;check(digit_core_policy_evaluate(&p,&r)&&r.decision==DIGIT_POLICY_DENY,"missing mission denied");
 digit_log_close();printf("Core provenance tests: %u executed, %u failed\n",count,failed);return failed!=0;
}
