/* [AI:GPT-6 | 2026-10-19] Isolated Core activation authorization regression.
 * Fixture contains no live module and opens no production resources.
 */
#include <stdio.h>
#include <string.h>
#include "module_registry.h"

static int failures, positives, negatives;
static void check(int condition,const char *description,int negative){
 if(negative)negatives++;else positives++;
 if(!condition){fprintf(stderr,"FAIL: %s\n",description);failures++;}
 else printf("PASS: %s\n",description);
}
int main(void){
 stnlabz_module_registry_t registry;
 stnlabz_module_record_t *record;
 stnlabz_module_descriptor_t descriptor={0};
 stnlabz_module_result_t rc;
 stnlabz_module_registry_init(&registry);
 strcpy(descriptor.id,"core-auth-fixture");
 strcpy(descriptor.name,"Core Authorization Fixture");
 descriptor.required_core_api_major=STNLABZ_MODULE_API_MAJOR;
 descriptor.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 /* A non-null qualification callback is required for discovery.
  * This fixture restores deterministic qualification evidence directly. */
 descriptor.qualify=(stnlabz_module_qualify_fn)NULL;
 /* Set a genuine callback below instead of bypassing discovery validation. */
 {
  extern stnlabz_module_result_t core_auth_fixture_qualify(stnlabz_module_qualification_result_t *);
  descriptor.qualify=core_auth_fixture_qualify;
 }
 rc=stnlabz_module_registry_discover(&registry,&descriptor);
 check(rc==STNLABZ_MODULE_OK,"Fixture discovered",0);
 if(rc!=STNLABZ_MODULE_OK)goto done;
 rc=stnlabz_module_registry_verify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"Fixture ABI verified",0);
 if(rc!=STNLABZ_MODULE_OK)goto done;
 rc=stnlabz_module_registry_qualify(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"Fixture qualified by Core",0);
 if(rc!=STNLABZ_MODULE_OK)goto done;
 record=registry.modules;
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_NOT_AUTHORIZED,
       "Activation without authorization rejected",1);
 check(record->state==STNLABZ_MODULE_STATE_QUALIFIED &&
       record->activation_authorized==0,
       "Unauthorized rejection preserves qualified state",1);
 rc=stnlabz_module_registry_authorize_activation(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK && record->activation_authorized==1,
       "Core grants explicit activation authority",0);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK && record->state==STNLABZ_MODULE_STATE_ACTIVE,
       "Authorized activation succeeds",0);
 rc=stnlabz_module_registry_stop(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK && record->state==STNLABZ_MODULE_STATE_STOPPED &&
       record->activation_authorized==0,
       "Stopping revokes activation authority",0);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_ERR_NOT_AUTHORIZED &&
       record->state==STNLABZ_MODULE_STATE_STOPPED,
       "Restart without renewed authority rejected without mutation",1);
 rc=stnlabz_module_registry_authorize_activation(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK,"Reauthorization accepted",0);
 rc=stnlabz_module_registry_activate(&registry,descriptor.id);
 check(rc==STNLABZ_MODULE_OK && record->state==STNLABZ_MODULE_STATE_ACTIVE,
       "Valid activation succeeds after prior rejection",0);
done:
 stnlabz_module_registry_destroy(&registry);
 printf("Core authorization: positive=%d negative=%d failures=%d\n",
        positives,negatives,failures);
 return failures?1:0;
}
stnlabz_module_result_t core_auth_fixture_qualify(stnlabz_module_qualification_result_t *r){
 if(!r)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(r,0,sizeof *r);
 r->tests_executed=20;r->tests_passed=20;
 r->negative_test_executed=1;r->negative_test_passed=1;
 return STNLABZ_MODULE_OK;
}
