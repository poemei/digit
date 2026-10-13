#include <stdio.h>
#include <string.h>
#include "code_reference.h"
static stnlabz_module_service_handler_fn service;
static unsigned checks,failed;
static int register_service(const char *n,stnlabz_module_service_handler_fn f,void *ctx){
 (void)ctx;if(strcmp(n,"code_reference.analyze"))return 0;service=f;return 1;
}
static int unregister_service(const char *n,void *ctx){
 (void)ctx;if(strcmp(n,"code_reference.analyze"))return 0;service=NULL;return 1;
}
static void check(int yes,const char *name){
 checks++;if(!yes){failed++;fprintf(stderr,"FAIL code reference: %s\n",name);}
}
int main(void){
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 stnlabz_module_host_t h={0};
 stnlabz_module_qualification_result_t q;
 code_reference_request_t req={0};code_reference_response_t res;
 size_t used=0;
 h.register_service=register_service;h.unregister_service=unregister_service;
 check(!strcmp(d->id,"code_reference"),"module id");
 check(d->version_major==0&&d->version_minor==1&&d->version_patch==1,"module version");
 check(d->qualify(&q)==STNLABZ_MODULE_OK&&q.tests_failed==0,"qualification");
 check(d->start(&h)==STNLABZ_MODULE_OK,"start");
 check(service!=NULL,"service published");
 req.contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 strcpy(req.language,"c");strcpy(req.source_name,"unseen.c");
 strcpy(req.source,"int increment(int value){ return value + 1; }");
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"valid C request");
 check(used==sizeof res&&res.status==CODE_REFERENCE_ANALYZED,"valid unfamiliar C accepted");
 strcpy(req.source,"int broken(void){ return missing_identifier; }");
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"invalid C request");
 check(res.status==CODE_REFERENCE_SOURCE_INVALID,"compiler detects undeclared identifier");
 check(strstr(res.diagnostics,"missing_identifier")!=NULL,"actual diagnostic identifies symbol");
 strcpy(req.source_name,"src/digit.c");
 strcpy(req.source,"#include \"agent_root.h\"\nint test_context(void){return 1;}\n");
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"project header analysis");
 check(res.status==CODE_REFERENCE_ANALYZED,"project header resolves via configured include directory");
 strcpy(req.source,"#include \"intentionally_nonexistent_header_65438.h\"\nint f(void){return 1;}\n");
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"missing header analysis");
 check(res.status==CODE_REFERENCE_ANALYSIS_INCOMPLETE,"missing header is context error not proven source defect");
 strcpy(req.language,"go");
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"future language request");
 check(res.status==CODE_REFERENCE_LANGUAGE_UNSUPPORTED,"Go not falsely supported");
 strcpy(req.language,"c");
 req.contract_version=99;
 check(service(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"contract mismatch rejected");
 req.contract_version=CODE_REFERENCE_CONTRACT_VERSION;
 check(service(&req,sizeof req,&res,sizeof res-1,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"short output rejected");
 check(d->stop()==STNLABZ_MODULE_OK,"stop");
 check(service==NULL,"service unpublished");
 printf("Code reference tests: %u executed, %u failed\n",checks,failed);
 printf("CODE REFERENCE TEST SUMMARY\nAssertions executed: %u\nAssertions passed: %u\nAssertions failed: %u\nOVERALL RESULT: %s\n",checks,checks-failed,failed,failed?"FAIL":"PASS");
 return failed?1:0;
}
