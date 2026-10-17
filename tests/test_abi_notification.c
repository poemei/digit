#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/abi.h"
static int calls[2], mode[2], checks, failures;
static stnlabz_module_result_t invoke(const char *name,const void *req,size_t size,
 void *response,size_t cap,size_t *used){
 int index;
 const stnlabz_module_notification_t *n=req;
 if(!strcmp(name,"notify.irc"))index=0;
 else if(!strcmp(name,"notify.slack"))index=1;
 else return STNLABZ_MODULE_ERR_NOT_FOUND;
 if(!mode[index])return STNLABZ_MODULE_ERR_NOT_FOUND;
 calls[index]++;
 if(size!=sizeof *n||n->version!=STNLABZ_NOTIFICATION_VERSION||
    strcmp(n->message,"event")||cap<sizeof(int))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *(int *)response=mode[index]==1?1:0;*used=sizeof(int);
 return mode[index]==3?STNLABZ_MODULE_ERR_START_FAILED:STNLABZ_MODULE_OK;
}
static int check(int yes,const char *why){checks++;if(!yes){failures++;fprintf(stderr,"notification test: %s\n",why);}return yes;}
int main(void){
 stnlabz_module_record_t records[3]={0};
 stnlabz_module_registry_t reg={0};
 stnlabz_module_host_t host={0};
 stnlabz_module_notification_t notice={0};
 stnlabz_notification_dispatch_result_t result={0};
 int ok=1;
 reg.modules=records;reg.count=3;host.invoke_service=invoke;
 strcpy(records[0].descriptor.id,"irc");records[0].state=STNLABZ_MODULE_STATE_ACTIVE;
 strcpy(records[1].descriptor.id,"slack");records[1].state=STNLABZ_MODULE_STATE_ACTIVE;
 strcpy(records[2].descriptor.id,"offline");records[2].state=STNLABZ_MODULE_STATE_STOPPED;
 notice.version=STNLABZ_NOTIFICATION_VERSION;strcpy(notice.message,"event");
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_OK
  &&result.eligible==2&&result.attempted==0,"neither available");
 mode[0]=1;
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_OK
  &&result.attempted==1&&result.delivered==1,"IRC only");
 mode[0]=0;mode[1]=1;
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_OK
  &&result.attempted==1&&result.delivered==1,"Slack only");
 mode[0]=1;
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_OK
  &&result.attempted==2&&result.delivered==2,"both");
 mode[0]=3;
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_OK
  &&result.attempted==2&&result.delivered==1&&result.failed==1,"failure isolated");
 notice.version=999;
 ok&=check(stnlabz_module_abi_notify(&reg,&host,&notice,&result)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"bad version");
 notice.version=STNLABZ_NOTIFICATION_VERSION;
 ok&=check(stnlabz_module_abi_notify(NULL,&host,&notice,&result)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"null registry");
 printf("ABI notification tests: %d executed, %d failed\n",checks,failures);
 return ok?0:1;
}
