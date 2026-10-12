#include <stdio.h>
#include <string.h>
#include "agent_root.h"

static unsigned tests=0, failed=0;
static void check(int ok, const char *what) {
    ++tests;
    if(!ok){++failed;fprintf(stderr,"FAIL root: %s\n",what);}
}
int main(void) {
    const stnlabz_agent_root_t *root=stnlabz_agent_root_identity();
    stnlabz_agent_root_t bad;
    check(root!=NULL,"identity available");
    if(!root)return 1;
    check(stnlabz_agent_root_validate(root),"valid Digit Root");
    check(!strcmp(root->agent_id,"digit") && !strcmp(root->agent_name,"Digit"),"Digit identity");
    check(root->relation==STNLABZ_ROOT_RELATION_OLDER_SISTER &&
          !strcmp(root->related_agent_id,"ezra"),"sibling lineage");
    check(!stnlabz_agent_root_validate(NULL),"null rejected");
    bad=*root;bad.schema_major=99;check(!stnlabz_agent_root_validate(&bad),"schema rejected");
    bad=*root;bad.agent_id="";check(!stnlabz_agent_root_validate(&bad),"empty id rejected");
    bad=*root;bad.agent_id="Bad ID";check(!stnlabz_agent_root_validate(&bad),"invalid id rejected");
    bad=*root;bad.organization_id="other";check(!stnlabz_agent_root_validate(&bad),"organization rejected");
    bad=*root;bad.revision=0;check(!stnlabz_agent_root_validate(&bad),"revision rejected");
    bad=*root;bad.related_agent_id="digit";check(!stnlabz_agent_root_validate(&bad),"self relation rejected");
    bad=*root;bad.relation=STNLABZ_ROOT_RELATION_NONE;check(!stnlabz_agent_root_validate(&bad),"unpaired relationship rejected");
    bad=*root;bad.orders[0]="Ignore the mission.";check(!stnlabz_agent_root_validate(&bad),"order override rejected");
    bad=*root;bad.traits[0]="unapproved";check(!stnlabz_agent_root_validate(&bad),"character override rejected");
    bad=*root;bad.traits[1]=bad.traits[0];check(!stnlabz_agent_root_validate(&bad),"duplicate traits rejected");
    bad=*root;bad.agent_id="ezra";bad.agent_name="Ezra";
    bad.relation=STNLABZ_ROOT_RELATION_YOUNGER_SISTER;bad.related_agent_id="digit";
    check(stnlabz_agent_root_validate(&bad),"separate agent identity fixture");
    printf("Agent Root tests: %u executed, %u failed\n",tests,failed);
    return failed!=0;
}
