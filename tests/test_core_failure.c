#include <stdio.h>
#include <string.h>
#include "core_policy.h"
#include "logging.h"
static unsigned count,failed;
static void check(int ok,const char *msg){++count;if(!ok){++failed;fprintf(stderr,"FAIL core failure: %s\n",msg);}}
int main(int argc,char **argv){
 digit_policy_request_t p={"operator","mission","module.activate","test",1,1,1,1,NULL,"fixture",1};digit_policy_result_t r;
 if(argc!=2)return 2;
 check(!digit_core_policy_evaluate(&p,&r),"policy cannot run without mandatory log");
 check(!digit_log_open_path(NULL),"null log path rejected");
 check(!digit_log_open_path("relative.log"),"relative test log rejected");
 check(digit_log_open_path(argv[1]),"valid test log opens");
 check(!digit_log_open_path(argv[1]),"duplicate open rejected");
 check(!digit_core_policy_evaluate(&p,NULL),"null result rejected");
 check(digit_core_policy_evaluate(NULL,&r)&&r.decision==DIGIT_POLICY_DENY,"missing input denied");
 check(digit_log_sync(),"operational evidence synchronized");
 digit_log_close();check(!digit_log_is_open(),"log closed");
 check(!digit_core_policy_evaluate(&p,&r),"policy fails closed after log close");
 printf("Core failure tests: %u executed, %u failed\n",count,failed);return failed!=0;
}
