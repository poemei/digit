/* [AI:GPT-6 | 2026-10-19] Isolated Core service registry regression.
 * Compile the real Core implementation in this test translation unit so the
 * static registration and dispatch functions under test are NOT replicas.
 * Never starts Digit or opens production services.
 */
#include <stdio.h>
#include <string.h>
#include "../core/digit.c"

static unsigned positive_count,negative_count,failures,invocations;
static void verify(int ok,const char *label,int negative){
 if(negative)negative_count++;else positive_count++;
 printf("%s: %s\n",ok?"PASS":"FAIL",label);
 if(!ok)failures++;
}
static stnlabz_module_result_t fixture_service(
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *response_used,void *context){
 const char *value=(const char *)context;
 invocations++;
 if(!request||request_size!=1||!response||response_size<2||
    !response_used||!value)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 ((char *)response)[0]=value[0];
 ((char *)response)[1]=0;
 *response_used=2;
 return STNLABZ_MODULE_OK;
}
int main(void){
 int owner_a=1,owner_b=2;
 char response[4]={0},request='x';
 size_t used=99;
 stnlabz_module_result_t rc;
 verify(!register_service(NULL,fixture_service,&owner_a),
        "NULL service name rejected",1);
 verify(!register_service("fixture.alpha",NULL,&owner_a),
        "NULL handler rejected",1);
 verify(!register_service("digit.message",fixture_service,&owner_a),
        "Core-owned message service cannot be overridden",1);
 verify(register_service("fixture.alpha",fixture_service,"A"),
        "Valid service registered",0);
 verify(!register_service("fixture.alpha",fixture_service,"B"),
        "Duplicate name rejected",1);
 rc=digit_service("fixture.alpha",&request,1,response,sizeof response,&used);
 verify(rc==STNLABZ_MODULE_OK&&used==2&&response[0]=='A'&&
        invocations==1,"Registered service invoked through Core",0);
 used=99;
 rc=digit_service("fixture.absent",&request,1,response,sizeof response,&used);
 verify(rc==STNLABZ_MODULE_ERR_NOT_FOUND&&used==0&&invocations==1,
        "Missing service returns NOT_FOUND without invoking handler",1);
 verify(!unregister_service("fixture.alpha","B"),
        "Wrong owner context cannot unregister service",1);
 used=0;response[0]=0;
 rc=digit_service("fixture.alpha",&request,1,response,sizeof response,&used);
 verify(rc==STNLABZ_MODULE_OK&&response[0]=='A'&&invocations==2,
        "Rejected unauthorized removal preserves service",0);
 verify(unregister_service("fixture.alpha","A"),
        "Matching owner unregisters service",0);
 used=99;
 rc=digit_service("fixture.alpha",&request,1,response,sizeof response,&used);
 verify(rc==STNLABZ_MODULE_ERR_NOT_FOUND&&used==0&&invocations==2,
        "Withdrawn service is no longer callable",1);
 verify(register_service("fixture.alpha",fixture_service,"B"),
        "Registry slot reusable after removal",0);
 used=0;response[0]=0;
 rc=digit_service("fixture.alpha",&request,1,response,sizeof response,&used);
 verify(rc==STNLABZ_MODULE_OK&&response[0]=='B'&&invocations==3,
        "Core remains operational after negative cases",0);
 verify(unregister_service("fixture.alpha","B"),
        "Fixture resources removed",0);
 printf("Core service registry: positive=%u negative=%u failures=%u\n",
        positive_count,negative_count,failures);
 return failures?1:0;
}
