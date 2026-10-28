#ifndef DIGIT_MODULE_HOST_H
#define DIGIT_MODULE_HOST_H

#include "stnlabz_abi/module.h"

const stnlabz_module_host_t *digit_module_host(void);
void digit_module_host_begin_registration(const char *module_id);
void digit_module_host_end_registration(void);

#endif
