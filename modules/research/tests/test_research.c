#include <stdio.h>
#include <string.h>
#include "research_contract.h"
#include "stnlabz_abi/module.h"
extern const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
extern int digit_research_url_allowed(const char *);
extern int digit_research_mission_allowed(const char *);
int main(void){
 stnlabz_module_qualification_result_t q={0};
 const stnlabz_module_descriptor_t *d=stnlabz_module_get_descriptor();
 if(!d||strcmp(d->id,"research")||d->version_major!=0||d->version_minor!=1||d->version_patch!=2)return 1;
 if(d->qualify(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 2;
 if(d->qualify(&q)!=STNLABZ_MODULE_OK||q.tests_executed<10||q.tests_failed||!q.negative_test_passed)return 3;
 if(d->start(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT)return 4;
 if(d->stop()!=STNLABZ_MODULE_ERR_INVALID_STATE)return 5;
 if(!digit_research_url_allowed("https://www.php.net/releases/"))return 6;
 if(digit_research_url_allowed("http://www.php.net")||digit_research_url_allowed("https://127.0.0.1/")||
    digit_research_url_allowed("https://localhost/")||digit_research_url_allowed("https://example.com@localhost/")||
    digit_research_url_allowed("https://[::1]/")||digit_research_url_allowed("https://example.com:8080/")||
    digit_research_url_allowed("https://example.com/%0aHeader"))return 7;
 if(!digit_research_mission_allowed("PHP security research")||digit_research_mission_allowed(" "))return 8;
 puts("Research 0.1.2 descriptor and bounded input tests: PASS");return 0;
}
