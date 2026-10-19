/* [AI:GPT-6 | 2026-10-19] Core hotload guard verification.
 * Includes actual Core hotload implementation: no surrogate version logic.
 * Never invokes replacement(), snapshot_candidate(), or production paths.
 */
#include <stdio.h>
#include <string.h>
#include "../core/core_hotload.c"
static unsigned positive,negative,failed;
static void check(int ok,const char *name,int is_negative){
 if(is_negative)negative++;else positive++;
 printf("%s: %s\n",ok?"PASS":"FAIL",name);
 if(!ok)failed++;
}
int main(void){
 stnlabz_hotload_t watch={0};
 stnlabz_module_descriptor_t incumbent={0},candidate={0};
 observed_t *known;
 const char original_hash[]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
 const char newer_hash[]="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
 strcpy(incumbent.id,"fixture");
 incumbent.version_major=1;incumbent.version_minor=5;incumbent.version_patch=7;
 check(remember(&watch,"fixture",original_hash,&incumbent),
       "Core records incumbent identity, SHA and version",0);
 known=find(&watch,"fixture");
 check(known&&known->major==1&&known->minor==5&&known->patch==7&&
       !strcmp(known->sha,original_hash),
       "Stored incumbent matches original descriptor and hash",0);
 candidate=incumbent;
 check(known&&!newer(known,&candidate),
       "Equal version cannot replace changed binary",1);
 candidate.version_patch=6;
 check(known&&!newer(known,&candidate),
       "Patch downgrade rejected",1);
 candidate.version_patch=8;
 check(known&&newer(known,&candidate),
       "Higher patch version accepted by version gate",0);
 candidate.version_minor=4;candidate.version_patch=99;
 check(known&&!newer(known,&candidate),
       "Lower minor version rejected despite higher patch",1);
 candidate.version_minor=6;candidate.version_patch=0;
 check(known&&newer(known,&candidate),
       "Higher minor version accepted",0);
 candidate.version_major=0;candidate.version_minor=99;candidate.version_patch=99;
 check(known&&!newer(known,&candidate),
       "Lower major version rejected despite larger subordinate versions",1);
 candidate.version_major=2;candidate.version_minor=0;candidate.version_patch=0;
 check(known&&newer(known,&candidate),
       "Higher major version accepted",0);
 check(known&&known->major==1&&known->minor==5&&known->patch==7&&
       !strcmp(known->sha,original_hash),
       "Rejected candidate checks preserve incumbent tracking",1);
 check(remember(&watch,"fixture",newer_hash,&candidate),
       "Core commits accepted tracking only on explicit remember",0);
 known=find(&watch,"fixture");
 check(known&&known->major==2&&known->minor==0&&known->patch==0&&
       !strcmp(known->sha,newer_hash)&&watch.count==1,
       "Updated record retains identity and replaces hash/version",0);
 check(find(&watch,"nonexistent")==NULL,
       "Unknown module lookup returns no incumbent",1);
 free(watch.seen);
 printf("Core hotload version guards: positive=%u negative=%u failures=%u\n",
        positive,negative,failed);
 return failed?1:0;
}
