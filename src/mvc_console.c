#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mvc_console.h"
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
 size_t i;
 if(!name||!name[0]||strlen(name)>DIGIT_ROUTER_PATH_MAX-2||
 !handler||command_count>=DIGIT_ROUTER_MAX_ROUTES)return 0;
 for(i=0;i<command_count;i++)if(!strcmp(commands[i].name,name))return 0;
 (void)snprintf(path,sizeof path,"/%s",name);
 (void)snprintf(service,sizeof service,"command.%s",name);
 if(!digit_router_bind(path,service))return 0;
 (void)snprintf(commands[command_count].name,sizeof commands[command_count].name,"%s",name);
 commands[command_count].handler=handler;
 commands[command_count++].context=context;
 return 1;
}
static int unregister_command(const char *name,void *context){
 size_t i;
 for(i=0;i<command_count;i++)if(!strcmp(commands[i].name,name)&&commands[i].context==context){
  commands[i].handler=NULL;return 1;
 }
 return 0;
}
static stnlabz_module_result_t invoke(const char *service,const void *request,size_t size,
 void *response,size_t capacity,size_t *used){
 size_t i;
 (void)response;(void)capacity;
 if(used)*used=0;
 if(!service||!request||size!=sizeof(stnlabz_module_command_t)||!used)
 return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 for(i=0;i<command_count;i++)if(commands[i].handler&&
 !strncmp(service,"command.",8)&&!strcmp(commands[i].name,service+8))
 return commands[i].handler((const stnlabz_module_command_t *)request,reply,NULL,commands[i].context);
 return STNLABZ_MODULE_ERR_NOT_FOUND;
}
static stnlabz_module_service_handler_fn reserved_handler;
static const char *reserved_name;
static void *reserved_context;
static int register_service(const char *name,stnlabz_module_service_handler_fn handler,void *ctx){
 if(!name||!handler||reserved_handler)return 0;
 reserved_name=name;reserved_handler=handler;reserved_context=ctx;return 1;
}
static int unregister_service(const char *name,void *ctx){
 if(!reserved_handler||strcmp(name,reserved_name)||ctx!=reserved_context)return 0;
 reserved_handler=NULL;reserved_name=NULL;reserved_context=NULL;return 1;
}
static const stnlabz_module_host_t host={
 NULL,NULL,register_command,unregister_command,register_service,unregister_service,invoke
};
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
 digit_router_reset();command_count=0;
 if(!digit_log_open()){fputs("Digit: mandatory log unavailable\n",stderr);return 1;}
 if(!digit_log_event("CONSOLE","START","mode=local")||!digit_log_sync()){good=0;goto done;}
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
   (void)digit_log_event("CONSOLE","MODULE_UNAVAILABLE",id);
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
 puts("Module commands are registered on activation. /quit exits.");
 fflush(stdout);
 while(fgets(line,sizeof line,stdin)){
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
