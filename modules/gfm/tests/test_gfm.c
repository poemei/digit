#include <stdio.h>
#include <string.h>
#include "gfm.h"
static unsigned checks,failures;
static void test(int ok,const char *name){
 checks++;if(!ok){failures++;fprintf(stderr,"FAIL gfm: %s\n",name);}
}
int main(void){
 digit_gfm_request_t req={0};digit_gfm_response_t res={0};
 size_t used=0;stnlabz_module_qualification_result_t q;
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 test(d&&d->version_major==0&&d->version_minor==1&&d->version_patch==0,"module version");
 test(d->qualify(&q)==STNLABZ_MODULE_OK&&q.tests_failed==0,"module qualification");
 req.contract_version=GFM_CONTRACT_VERSION;
 snprintf(req.document,sizeof req.document,
  "# Heading\n- Item\n| A | B |\n|:--|:--|\n[Link](url)\n~~~c\n# not heading\n~~~\n");
 test(digit_gfm_analyze(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK,"analyzes bounded document");
 test(res.headings==1&&res.lists==1&&res.code_fences==1&&res.code_lines==1,"ignores headings inside fenced code");
 test(res.balanced_fences&&res.links==1&&res.tables==1,"detects link and table candidates");
 test(used==sizeof res,"response contract");
 snprintf(req.document,sizeof req.document,"~~~\n# code\n");
 test(digit_gfm_analyze(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_OK&&!res.balanced_fences,"unclosed fence detected");
 req.contract_version=9;
 test(digit_gfm_analyze(&req,sizeof req,&res,sizeof res,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,"wrong contract denied");
 printf("GFM tests: %u executed, %u failed\n",checks,failures);
 return failures?1:0;
}
