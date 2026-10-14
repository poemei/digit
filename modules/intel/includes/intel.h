#ifndef DIGIT_INTEL_MODULE_H
#define DIGIT_INTEL_MODULE_H
#include "stnlabz_abi/module.h"
#define DIGIT_INTEL_TEXT_MAX 4096
typedef struct {char command[32];char actor[64];} digit_intel_request_t;
typedef struct {unsigned int sources_ok,sources_failed;char report[DIGIT_INTEL_TEXT_MAX];} digit_intel_result_t;
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
#endif
