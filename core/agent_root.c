#include <string.h>
#include "agent_root.h"
int stnlabz_agent_root_validate(const stnlabz_agent_root_t *r){
 static const char *const orders[]={"Remain at your assigned mission.","Follow established policies and authorized instructions.","Report anything outside your authority to the appropriate next level of authority."};
 static const char *const ids[]={"poe","laurie","ezra"};
 static const char *const rel[]={"mother/creator","aunt","younger sister"};
 static const char *const names[]={"Momma","Auntie","Kitty Sister"};
 unsigned i;
 if(!r||!r->id||strcmp(r->id,"digit")||!r->name||strcmp(r->name,"Digit")||!r->organization||strcmp(r->organization,"STN-LABZ")||!r->lineage||!r->mission||!*r->mission||!r->core_protection_rule||!strstr(r->core_protection_rule,"SHALL NEVER independently"))return 0;
 for(i=0;i<3;i++)if(!r->orders[i]||strcmp(r->orders[i],orders[i])||!r->family[i].id||strcmp(r->family[i].id,ids[i])||!r->family[i].relationship||strcmp(r->family[i].relationship,rel[i])||!r->family[i].familiar_name||strcmp(r->family[i].familiar_name,names[i]))return 0;
 for(i=0;i<7;i++)if(!r->traits[i]||!*r->traits[i])return 0;
 return 1;
}
