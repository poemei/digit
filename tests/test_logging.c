#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
static unsigned tests,failed;
static void check(int ok,const char *name){
 ++tests;
 if(!ok){++failed;fprintf(stderr,"FAIL logging: %s\n",name);}
}
int main(void){
 char directory[]="/tmp/digit-log-XXXXXX",core[512],module[512],line[512];
 FILE *f;
 if(!mkdtemp(directory))return 1;
 snprintf(core,sizeof core,"%s/digit.log",directory);
 snprintf(module,sizeof module,"%s/modules.log",directory);
 check(!strcmp(DIGIT_LOG_PATH,"./logs/digit.log"),"Core log path");
 check(!strcmp(DIGIT_MODULE_LOG_PATH,"./logs/modules.log"),"module log path");
 check(!digit_log_event("CORE","PREOPEN",NULL),"preopen denied");
 check(!digit_log_open_path("relative"),"relative rejected");
 check(digit_log_open_path(core),"both logs opened");
 check(digit_log_is_open(),"open state");
 check(!digit_log_open_path(core),"second open denied");
 check(digit_log_event("CORE","START","test=true"),"Core recorded");
 check(digit_log_event("MODULE","QUALIFICATION_GREEN","module=expression version=0.2.1"),"module recorded");
 check(digit_log_event("MODULE","VERIFY_PASS","module=expression"),"routine verified");
 check(digit_log_event("MODULE","HOTLOAD_ACTIVE","module=expression version=0.2.1 result=OK"),"active recorded");
 check(digit_log_event("MODULE","MESSAGE","[EXPRESSION] Ready"),"module message recorded");
 check(digit_log_event("ROUTER","DISPATCH","request=1"),"router recorded");
 check(digit_log_sync(),"both logs synced");
 digit_log_close();
 check(!digit_log_is_open(),"closed");
 f=fopen(core,"r");
 check(f!=NULL,"Core file exists");
 if(f){
  check(fgets(line,sizeof line,f)&&strstr(line,"CORE START test=true"),"Core in digit.log");
  check(fgets(line,sizeof line,f)&&strstr(line,"ROUTER DISPATCH request=1"),"Router in digit.log");
  check(fgets(line,sizeof line,f)==NULL,"no module traffic in digit.log");
  fclose(f);
 }
 f=fopen(module,"r");
 check(f!=NULL,"module file exists");
 if(f){
  check(fgets(line,sizeof line,f)&&strstr(line,"MODULE QUALIFICATION_GREEN sequence=")&&strstr(line,"module=expression version=0.2.1"),"qualification in modules.log");
  check(fgets(line,sizeof line,f)&&strstr(line,"MODULE HOTLOAD_ACTIVE sequence=")&&
        strstr(line,"module=expression version=0.2.1"),"concise activation");
  check(fgets(line,sizeof line,f)&&strstr(line,"MODULE MESSAGE [EXPRESSION] Ready"),
        "operator module message");
  check(fgets(line,sizeof line,f)==NULL,"routine verification hidden; no Core traffic in modules.log");
  fclose(f);
 }
 unlink(core);unlink(module);rmdir(directory);
 printf("Core logging tests: %u executed, %u failed\n",tests,failed);
 return failed!=0;
}
