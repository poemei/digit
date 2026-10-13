#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "runtime.h"
/* Opaque Core-mediated service response; no DSD linkage or dependency. */
typedef struct {unsigned int contract_version;char text[2048];} console_dsd_alert_t;
#include "mvc_console.h"
#include "agent_root.h"
#include "router.h"
#include "core_activation.h"
#include "core_discovery.h"
#include "logging.h"
#include "stnlabz_abi/module_loader.h"
#include "stnlabz_abi/module_discovery.h"

typedef struct {
 char name[STNLABZ_MODULE_COMMAND_NAME_MAX];
 stnlabz_module_command_handler_fn handler;
 void *context;
} command_entry_t;
static command_entry_t commands[DIGIT_ROUTER_MAX_ROUTES];
static size_t command_count;
static int reply(void *context,const char *message){
 (void)context;
 if(!message)return 0;
 puts(message);fflush(stdout);return 1;
}
static int register_command(const char *name,stnlabz_module_command_handler_fn handler,void *context){
 char path[DIGIT_ROUTER_PATH_MAX],service[STNLABZ_MODULE_SERVICE_NAME_MAX];
 size_t i,slot=DIGIT_ROUTER_MAX_ROUTES;
 if(!name||!name[0]||strlen(name)>DIGIT_ROUTER_PATH_MAX-2||!handler)return 0;
 for(i=0;i<command_count;i++){
  if(commands[i].handler&&!strcmp(commands[i].name,name))return 0;
  if(!commands[i].handler&&slot==DIGIT_ROUTER_MAX_ROUTES)slot=i;
 }
 if(slot==DIGIT_ROUTER_MAX_ROUTES){
  if(command_count>=DIGIT_ROUTER_MAX_ROUTES)return 0;
  slot=command_count;
 }
 (void)snprintf(path,sizeof path,"/%s",name);
 (void)snprintf(service,sizeof service,"command.%s",name);
 if(!digit_router_bind(path,service))return 0;
 (void)snprintf(commands[slot].name,sizeof commands[slot].name,"%s",name);
 commands[slot].handler=handler;
 commands[slot].context=context;
 if(slot==command_count)command_count++;
 return 1;
}
static int unregister_command(const char *name,void *context){
 size_t i;
 for(i=0;i<command_count;i++)if(!strcmp(commands[i].name,name)&&commands[i].context==context){
  char path[DIGIT_ROUTER_PATH_MAX],service[STNLABZ_MODULE_SERVICE_NAME_MAX];
  (void)snprintf(path,sizeof path,"/%s",name);
  (void)snprintf(service,sizeof service,"command.%s",name);
  if(!digit_router_unbind(path,service))return 0;
  commands[i].handler=NULL;return 1;
 }
 return 0;
}
typedef struct {char name[STNLABZ_MODULE_SERVICE_NAME_MAX];stnlabz_module_service_handler_fn handler;void *context;} service_entry_t;
static service_entry_t services[32];
static size_t service_count;
static stnlabz_module_result_t invoke(const char *service,const void *request,size_t size,
 void *response,size_t capacity,size_t *used){
 size_t i;
 if(used)*used=0;
 if(!service||!request||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!strncmp(service,"command.",8)){
  if(size!=sizeof(stnlabz_module_command_t))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  for(i=0;i<command_count;i++)if(commands[i].handler&&
   !strcmp(commands[i].name,service+8))
   return commands[i].handler((const stnlabz_module_command_t *)request,reply,NULL,commands[i].context);
  return STNLABZ_MODULE_ERR_NOT_FOUND;
 }
 for(i=0;i<service_count;i++)if(services[i].handler&&!strcmp(services[i].name,service))
  return services[i].handler(request,size,response,capacity,used,services[i].context);
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static int register_service(const char *name,stnlabz_module_service_handler_fn handler,void *ctx){
 size_t i;
 if(!name||!name[0]||strlen(name)>=sizeof services[0].name||!handler||service_count>=32)return 0;
 for(i=0;i<service_count;i++)if(!strcmp(services[i].name,name))return 0;
 (void)snprintf(services[service_count].name,sizeof services[0].name,"%s",name);
 services[service_count].handler=handler;services[service_count++].context=ctx;
 if(!strncmp(name,"route.",6)&&!digit_router_bind(name+6,name)){
  --service_count;memset(&services[service_count],0,sizeof services[0]);return 0;
 }
 return 1;
}
static int unregister_service(const char *name,void *ctx){
 size_t i;
 if(!name)return 0;
 for(i=0;i<service_count;i++)if(!strcmp(services[i].name,name)&&services[i].context==ctx){
  if(!strncmp(name,"route.",6)&&!digit_router_unbind(name+6,name))return 0;
  if(i+1<service_count)memmove(&services[i],&services[i+1],(service_count-i-1)*sizeof services[0]);
  --service_count;memset(&services[service_count],0,sizeof services[0]);return 1;
 }
 return 0;
}
/* Read-only Core identity projection; no Root mutation or authority grant. */
static stnlabz_module_result_t core_identity_service(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *context){
 const stnlabz_agent_root_t *root=stnlabz_agent_root_identity();
 int n;
 (void)context;
 if(!request||size!=1||*(const char *)request!=0||!response||!used||
    !stnlabz_agent_root_validate(root))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 n=snprintf(response,capacity,
  "I am %s, the STN-LABZ %s. My role is defined by my Core.",
  root->agent_name,root->lineage);
 if(n<0||(size_t)n>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *used=(size_t)n+1;return STNLABZ_MODULE_OK;
}
static int module_message(const char *message){
 return message&&digit_log_event("MODULE","MESSAGE",message)&&digit_log_sync();
}
static const stnlabz_module_host_t host={
 module_message,NULL,register_command,unregister_command,register_service,unregister_service,invoke
};
static void console_dsd_watch(void){
 static int previously_reported;
 unsigned int version=1U;console_dsd_alert_t result={0};
 size_t used=0;
 if(invoke("dsd.watch",&version,sizeof version,&result,sizeof result,&used)==STNLABZ_MODULE_OK&&
    used==sizeof result&&result.contract_version==1U&&
    memchr(result.text,0,sizeof result.text)&&result.text[0]){
  if(previously_reported)putchar('\n');
  puts(result.text);
  previously_reported=1;
  fflush(stdout);
 }
}
/* systemd StandardInput=null is /dev/null; scripted console pipes must remain interactive. */
static int unattended_service_input(void){
 struct stat input,devnull;
 return fstat(STDIN_FILENO,&input)==0 && stat("/dev/null",&devnull)==0 &&
        S_ISCHR(input.st_mode) && S_ISCHR(devnull.st_mode) &&
        input.st_rdev==devnull.st_rdev;
}
int digit_mvc_console(void){
 stnlabz_module_registry_t registry;
 stnlabz_module_loader_t loader;
 stnlabz_module_discovery_report_t discovered;
 digit_policy_request_t policy={"local-console","console","module.activate",NULL,
  1,1,1,1,NULL,"trusted-local-console",1};
 char directory[1024],line[512];
 size_t seq=0,i;
 int good=1,router_active=0;
 stnlabz_module_registry_init(&registry);
 stnlabz_module_loader_init(&loader);
 digit_router_reset();command_count=0;service_count=0;
 if(!register_service("core.identity.read",core_identity_service,NULL))return 1;
 if(!digit_log_open()){fputs("Digit: mandatory log unavailable\n",stderr);return 1;}
 if(!digit_log_event("CONSOLE","START","mode=local")||!digit_log_sync()){good=0;goto done;}
 if(!stnlabz_agent_root_validate(stnlabz_agent_root_identity())){
  fputs("Digit: Agent Root authority validation failed\n",stderr);
  (void)digit_log_event("CONSOLE","ROOT_REJECTED","authority_rule_invalid");
  good=0;goto done;
 }
 if(stnlabz_module_discovery_get_path(directory,sizeof directory)!=STNLABZ_MODULE_OK ||
 !digit_core_discover(&registry,&loader,directory,&discovered) ||
 !digit_core_verify(&registry)||!digit_core_qualify(&registry)){good=0;goto done;}
 /* Module capabilities register their own commands at authorized activation.
    Router receives the resulting generic bindings; there are no module names here. */
 for(i=0;i<registry.count;i++){
  const char *id=registry.modules[i].descriptor.id;
  if(registry.modules[i].state!=STNLABZ_MODULE_STATE_QUALIFIED)continue;
  policy.module_id=id;
  if(digit_core_authorize(&registry,id,&policy)!=STNLABZ_MODULE_OK ||
     digit_core_activate(&registry,id,&host,&policy)!=STNLABZ_MODULE_OK)
   (void)digit_log_event("MODULE","UNAVAILABLE",id);
 }
 /* Router is infrastructure controlled by Digit; no Bootstrap intermediary. */
 {
  stnlabz_module_descriptor_t router=digit_router_descriptor();
  policy.module_id="router";
  if(stnlabz_module_registry_discover(&registry,&router)!=STNLABZ_MODULE_OK ||
     stnlabz_module_registry_verify(&registry,"router")!=STNLABZ_MODULE_OK ||
     stnlabz_module_registry_qualify(&registry,"router")!=STNLABZ_MODULE_OK ||
     digit_core_authorize(&registry,"router",&policy)!=STNLABZ_MODULE_OK ||
     digit_core_activate(&registry,"router",&host,&policy)!=STNLABZ_MODULE_OK){
   good=0;goto done;
  }
  router_active=1;
 }
 puts("STN-LABZ Digit | interactive console");
 puts("CORE AUTHORITY RULE:");
 puts(stnlabz_agent_root_identity()->core_modification_rule);
 puts("Module commands are registered on activation. /quit exits.");
 fflush(stdout);
 if(unattended_service_input()){
  digit_runtime_t runtime;
  digit_runtime_init(&runtime);
  if(!digit_log_event("CORE","SERVICE_READY","mode=unattended")||
     !digit_log_sync()||!digit_runtime_run(&runtime))good=0;
  goto done;
 }
 console_dsd_watch();
 for(;;){
  fd_set readfds;struct timeval timeout;
  int ready;
  FD_ZERO(&readfds);FD_SET(STDIN_FILENO,&readfds);
  timeout.tv_sec=5;timeout.tv_usec=0;
  ready=select(STDIN_FILENO+1,&readfds,NULL,NULL,&timeout);
  if(ready==0){console_dsd_watch();continue;}
  if(ready<0){if(errno==EINTR)continue;good=0;break;}
  if(!fgets(line,sizeof line,stdin))break;
  stnlabz_module_command_t command={0};
  digit_router_request_t request={0};
  digit_router_result_t result={0};
  char word[STNLABZ_MODULE_COMMAND_NAME_MAX];
  size_t j=0,start=0,len=strlen(line);
  if(len&&!strchr(line,'\n')&&!feof(stdin)){
   int c;while((c=getchar())!='\n'&&c!=EOF){}
   puts("Input too long.");continue;
  }
  while(len&&(line[len-1]=='\n'||line[len-1]=='\r'))line[--len]=0;
  if(!strcmp(line,"/quit")||!strcmp(line,"quit"))break;
  while(line[start]==' '||line[start]=='/')start++;
  while(line[start+j]&&line[start+j]!=' '&&j<sizeof word-1){word[j]=line[start+j];j++;}
  word[j]=0;
  if(!j){continue;}
  if(line[start+j]&&line[start+j]!=' '){puts("Invalid command.");continue;}
  (void)snprintf(command.sender,sizeof command.sender,"local-console");
  (void)snprintf(command.account,sizeof command.account,"local");
  (void)snprintf(command.name,sizeof command.name,"%s",word);
  if(line[start+j]==' ') (void)snprintf(command.arguments,sizeof command.arguments,"%s",line+start+j+1);
  (void)snprintf(request.request_id,sizeof request.request_id,"console-%zu",++seq);
  (void)snprintf(request.path,sizeof request.path,"/%s",word);
  request.core_authorized=1; /* Local in-process adapter, not remote authentication. */
  request.payload=&command;request.payload_size=sizeof command;
  switch(digit_router_dispatch(&request,NULL,0,&result)){
   case DIGIT_ROUTE_OK:break;
   case DIGIT_ROUTE_UNKNOWN:puts("Unknown command.");break;
   case DIGIT_ROUTE_UNAVAILABLE:puts("Module unavailable.");break;
   case DIGIT_ROUTE_DENIED:puts("Request denied.");break;
   default:puts("Command failed; inspect logs/digit.log.");break;
  }
  fflush(stdout);
 }
done:
 if(router_active&&!digit_core_stop(&registry,"router"))good=0;
 if(!digit_core_stop_all(&registry))good=0;
 if(!digit_log_event("CONSOLE",good?"STOPPED":"FAILED","mode=local")||!digit_log_sync())good=0;
 if(good)digit_core_discovery_cleanup(&registry,&loader);
 digit_log_close();
 return good?0:1;
}
