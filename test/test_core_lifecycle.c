/* [AI:GPT-6 | 2026-10-19] Isolated Core lifecycle integrity regression.
 * Exercises actual Core registry state transitions; no live module or DB.
 */
#include <stdio.h>
#include <string.h>
#include "module_registry.h"

static unsigned positive,negative,failures;
static stnlabz_module_result_t qualify_fixture(stnlabz_module_qualification_result_t *r){
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 r->tests_executed=20;r->tests_passed=20;
 r->negative_test_executed=1;r->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
static void check(int ok,const char *label,int is_negative){
 if(is_negative)negative++;else positive++;
 printf("%s: %s\n",ok?"PASS":"FAIL",label);
 if(!ok)failures++;
}
int main(void){
 stnlabz_module_registry_t registry;
 stnlabz_module_descriptor_t descriptor={0};
 const stnlabz_module_record_t *entry;
 stnlabz_module_result_t rc;
 stnlabz_module_registry_init(&registry);
 strcpy(descriptor.id,"lifecycle-fixture");
 strcpy(descriptor.name,"Core Lifecycle Fixture");
 descriptor.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 descriptor.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 descriptor.qualify=qualify_fixture;

 rc=stnlabz_module_registry_discover(&registry,&descriptor);
 check(rc==STNLABZ_MODULE_OK,"Discovery accepted",0);
 entry=stnlabz_module_registry_find(&registry,descriptor.id);
 check(entry&&entry->state==STNLABZ_MODULE_STATE_DISCOVERED,
       "New record begins DISCOVERED",0);
 if(!entry)goto done;

 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_NOT_QUALIFIED &&
       entry->state==STNLABZ_MODULE_STATE_DISCOVERED,
       "DISCOVERED activation rejected without state mutation",1);
 rc=stnlabz_module_registry_stop(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_INVALID_STATE &&
       entry->state==STNLABZ_MODULE_STATE_DISCOVERED,
       "DISCOVERED stop rejected without state mutation",1);
 rc=stnlabz_module_registry_verify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_UNVERIFIED,
       "Verified record transitions to UNVERIFIED",0);
 rc=stnlabz_module_registry_verify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_INVALID_STATE &&
       entry->state==STNLABZ_MODULE_STATE_UNVERIFIED,
       "Duplicate verification rejected",1);
 rc=stnlabz_module_registry_qualify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_QUALIFIED,
       "Qualification transitions to QUALIFIED",0);
 rc=stnlabz_module_registry_qualify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_INVALID_STATE &&
       entry->state==STNLABZ_MODULE_STATE_QUALIFIED,
       "Duplicate qualification rejected",1);
 rc=stnlabz_module_registry_unregister(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&registry.count==0,
       "Non-running QUALIFIED record can be unregistered",0);

 rc=stnlabz_module_registry_discover(&registry,&descriptor);
 check(rc==STNLABZ_MODULE_OK,"Fresh lifecycle fixture discovered",0);
 rc=stnlabz_module_registry_verify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"Fresh lifecycle fixture verified",0);
 rc=stnlabz_module_registry_qualify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"Fresh lifecycle fixture qualified",0);
 entry=stnlabz_module_registry_find(&registry,descriptor.id);
 if(!entry)goto done;
 rc=stnlabz_module_registry_authorize_activation(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->activation_authorized,
       "Qualified record explicitly authorized",0);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_ACTIVE,
       "Authorized record enters ACTIVE",0);
 rc=stnlabz_module_registry_unregister(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_INVALID_STATE&&
       registry.count==1&&entry->state==STNLABZ_MODULE_STATE_ACTIVE,
       "ACTIVE unregistration rejected without record loss",1);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_NOT_QUALIFIED&&
       entry->state==STNLABZ_MODULE_STATE_ACTIVE,
       "Duplicate activation rejected without state mutation",1);
 rc=stnlabz_module_registry_stop(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_STOPPED&&
       !entry->activation_authorized,
       "ACTIVE stop transitions to STOPPED and revokes authority",0);
 rc=stnlabz_module_registry_stop(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_INVALID_STATE&&
       entry->state==STNLABZ_MODULE_STATE_STOPPED,
       "Duplicate stop rejected without state mutation",1);
 rc=stnlabz_module_registry_authorize_activation(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"STOPPED fixture can be reauthorized",0);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_ACTIVE,
       "Valid recovery from STOPPED to ACTIVE",0);
 rc=stnlabz_module_registry_quarantine(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK&&entry->state==STNLABZ_MODULE_STATE_QUARANTINED&&
       !entry->activation_authorized,
       "Quarantine revokes authority and enters QUARANTINED",0);
 rc=stnlabz_module_registry_authorize_activation(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_QUARANTINED&&
       entry->state==STNLABZ_MODULE_STATE_QUARANTINED,
       "Quarantine blocks reauthorization without state mutation",1);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_QUARANTINED&&
       entry->state==STNLABZ_MODULE_STATE_QUARANTINED,
       "Quarantine blocks activation without state mutation",1);
done:
 stnlabz_module_registry_destroy(&registry);
 printf("Core lifecycle: positive=%u negative=%u failures=%u\n",
        positive,negative,failures);
 return failures?1:0;
}
