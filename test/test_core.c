#include <stdio.h>
#include <string.h>
#include "agent_root.h"
#include "report.h"
#include "irc_render.h"
int main(void){
 char answer[512];const stnlabz_agent_root_t *root=stnlabz_agent_root_identity();
 if(!stnlabz_agent_root_validate(root))return 1;
 if(strcmp(root->orders[0],"Remain at your assigned mission."))return 2;
 if(strcmp(root->orders[1],"Follow established policies and authorized instructions."))return 3;
 if(strcmp(root->orders[2],"Report anything outside your authority to the appropriate next level of authority."))return 4;
 if(strcmp(root->family[0].familiar_name,"Momma")||strcmp(root->family[1].familiar_name,"Auntie")||strcmp(root->family[2].familiar_name,"Kitty Sister"))return 5;
 if(digit_report_classify("Digit: what is your mission?")!=DIGIT_REPORT_MISSION)return 6;
 if(!digit_report_line(DIGIT_REPORT_ORDERS,2,answer,sizeof answer))return 7;
 if(!strstr(answer,"Report anything outside your authority"))return 8;
 if(!digit_irc_render(answer,sizeof answer,"#stn-labz","test"))return 9;
 puts("Core reporting tests passed");return 0;
}
