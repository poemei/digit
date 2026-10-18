#include <stdio.h>
#include "agent_root.h"
#include "irc.h"
int main(void){
 if(!stnlabz_agent_root_validate(stnlabz_agent_root_identity())){
  fputs("Digit: Core Agent Root validation failed\n",stderr);return 1;
 }
 return digit_irc_run();
}
