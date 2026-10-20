#ifndef DIGIT_CORE_DSD_H
#define DIGIT_CORE_DSD_H
#include "stnlabz_abi/module.h"
int core_dsd_handles(const char *name);
stnlabz_module_result_t core_dsd_dispatch(const char *name,
 const void *request,size_t request_size,void *response,
 size_t response_size,size_t *used);
void core_dsd_service_result(const char *service,
 stnlabz_module_result_t rc,size_t response_bytes,unsigned line);
#endif
