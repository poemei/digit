#ifndef STNLABZ_AGENT_ROOT_H
#define STNLABZ_AGENT_ROOT_H
#include <stddef.h>
#define STNLABZ_ROOT_TEXT_MAX 128U
#define STNLABZ_ROOT_ORDER_COUNT 3U
#define STNLABZ_ROOT_FAMILY_COUNT 3U
#define STNLABZ_ROOT_TRAIT_COUNT 7U
typedef struct {const char *id,*relationship,*familiar_name;} stnlabz_family_member_t;
typedef struct {
 const char *id,*name,*organization,*lineage,*mission;
 const char *orders[STNLABZ_ROOT_ORDER_COUNT];
 const char *core_protection_rule;
 const char *traits[STNLABZ_ROOT_TRAIT_COUNT];
 stnlabz_family_member_t family[STNLABZ_ROOT_FAMILY_COUNT];
} stnlabz_agent_root_t;
const stnlabz_agent_root_t *stnlabz_agent_root_identity(void);
int stnlabz_agent_root_validate(const stnlabz_agent_root_t *);
#endif
