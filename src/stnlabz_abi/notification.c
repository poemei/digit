/* STN-LABZ generic notification fan-out through ABI service registry.
 * No knowledge of transport names, payload rendering, or network protocols. */
#include <stdio.h>
#include <string.h>
#include "stnlabz_abi/abi.h"

stnlabz_module_result_t stnlabz_module_abi_notify(
 const stnlabz_module_registry_t *registry,
 const stnlabz_module_host_t *host,
 const stnlabz_module_notification_t *notification,
 stnlabz_notification_dispatch_result_t *result)
{
 size_t i;
 if(!registry||!host||!host->invoke_service||!notification||!result||
    notification->version!=STNLABZ_NOTIFICATION_VERSION||
    !memchr(notification->message,0,sizeof notification->message))
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(result,0,sizeof *result);
 for(i=0;i<registry->count;i++){
  const stnlabz_module_record_t *r=&registry->modules[i];
  char service[STNLABZ_MODULE_SERVICE_NAME_MAX];
  int confirmed=0;size_t used=0;
  stnlabz_module_result_t rc;
  if(r->state!=STNLABZ_MODULE_STATE_ACTIVE)continue;
  if(snprintf(service,sizeof service,"notify.%s",r->descriptor.id)>=(int)sizeof service)
   continue;
  result->eligible++;
  rc=host->invoke_service(service,notification,sizeof *notification,
   &confirmed,sizeof confirmed,&used);
  if(rc==STNLABZ_MODULE_ERR_NOT_FOUND)continue; /* Optional subscriber */
  result->attempted++;
  if(rc==STNLABZ_MODULE_OK&&used==sizeof confirmed&&confirmed==1)
   result->delivered++;
  else result->failed++;
 }
 return STNLABZ_MODULE_OK;
}
