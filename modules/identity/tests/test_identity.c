#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include "identity_model.h"
int main(void)
{
 unsigned executed=0,passed=0;
 int negative_executed=0,negative_passed=0;
 identity_record_t record={0};
 char answer[1200];
 int ok=identity_selftest(&executed,&passed,&negative_executed,&negative_passed);
 printf("Identity qualification executed=%u passed=%u negative=%d/%d\n",
        executed,passed,negative_passed,negative_executed);
 if (!identity_load("data/identity.conf",&record)) {
  puts("Identity record load FAIL");
  return 1;
 }
 if (!identity_reply(&record,"mission",answer,sizeof answer) ||
     strcmp(answer,"Digit's mission is to support STN-LABZ in the pursuit and advancement of its overall mission, objectives, and long-term vision through research, development, engineering, technical analysis, innovation, and operational support."))
  ok=0;
 puts(ok?"Identity approved mission and qualification PASS":"Identity approved mission / qualification FAIL");
 return ok?0:1;
}
