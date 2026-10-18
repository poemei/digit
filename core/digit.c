#include <stdio.h>
#include "agent_root.h"
#include "digit.h"
#include "logging.h"
#include "runtime.h"
#include "core_discovery.h"
#include "core_activation.h"
#include "core_inventory.h"
#include "stnlabz_abi/module_registry.h"
#include "stnlabz_abi/module_loader.h"
int stnlabz_initialize(void){
 stnlabz_runtime_t runtime;
 stnlabz_module_registry_t modules;
 stnlabz_module_loader_t loader;
 stnlabz_module_discovery_report_t discovery;
 char path[1024];
 const stnlabz_agent_root_t *root=stnlabz_agent_root_identity();
 stnlabz_module_registry_init(&modules);
 stnlabz_module_loader_init(&loader);
 if(!stnlabz_log_open())return 1;
 if(!stnlabz_agent_root_validate(root)){(void)stnlabz_log_event("CORE","ROOT_REJECTED","validation=failed");stnlabz_log_close();return 1;}
 if(!stnlabz_log_event("CORE","ROOT_VERIFIED","agent=digit revision=1"))goto fail;
 stnlabz_runtime_init(&runtime);
 if(stnlabz_module_discovery_get_path(path,sizeof path)!=STNLABZ_MODULE_OK||
 !stnlabz_core_discover(&modules,&loader,path,&discovery)||
 !stnlabz_core_verify(&modules)||!stnlabz_core_qualify(&modules)||
 !stnlabz_core_inventory_log(&modules))goto fail;
 puts("STN-LABZ Digit | Core verified");
 if(!stnlabz_log_sync()||!stnlabz_runtime_run(&runtime))goto fail;
 (void)stnlabz_core_stop_all(&modules);
 stnlabz_core_discovery_cleanup(&modules,&loader);
 stnlabz_log_close();return 0;
fail:
 (void)stnlabz_core_stop_all(&modules);
 stnlabz_core_discovery_cleanup(&modules,&loader);
 stnlabz_log_close();return 1;
}
