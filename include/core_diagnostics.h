#ifndef DIGIT_CORE_DIAGNOSTICS_H
#define DIGIT_CORE_DIAGNOSTICS_H

#include <stddef.h>
#include "stnlabz_abi/module.h"

#define DIGIT_DIAG_TEXT_MAX 2048
#define DIGIT_DIAG_SERVICE_MAX 16
#define DIGIT_DIAG_TRACE_MAX 16

void digit_diag_begin(const char *sender, const char *account, const char *text);
void digit_diag_route(const char *command);
void digit_diag_command(const char *command, const char *owner,
                        stnlabz_module_result_t result, int replied);
void digit_diag_service(const char *service, const char *owner,
                        stnlabz_module_result_t result);
int digit_diag_render(const char *sender, const char *account,
                      char *output, size_t output_size);

#endif
