#ifndef DIGIT_CODE_REFERENCE_H
#define DIGIT_CODE_REFERENCE_H
#include "stnlabz_abi/module.h"

/* ABI 1.5 opaque service payload, contract version 1.
   The language is a string, not an enum: future backends may be added
   without changing the wire shape.  All strings must be NUL terminated. */
#define CODE_REFERENCE_SOURCE_MAX 4096
#define CODE_REFERENCE_DIAGNOSTIC_MAX 4096
#define CODE_REFERENCE_CONTRACT_VERSION 1U
typedef enum {
 CODE_REFERENCE_ANALYZED=0,
 CODE_REFERENCE_SOURCE_INVALID=1,
 CODE_REFERENCE_LANGUAGE_UNSUPPORTED=2,
 CODE_REFERENCE_ANALYZER_UNAVAILABLE=3,
 CODE_REFERENCE_ANALYSIS_INCOMPLETE=4
} code_reference_status_t;
typedef struct {
 unsigned int contract_version;
 char language[32];
 char source_name[128];
 char source[CODE_REFERENCE_SOURCE_MAX];
} code_reference_request_t;
typedef struct {
 unsigned int contract_version;
 code_reference_status_t status;
 char analyzer[64];
 char diagnostics[CODE_REFERENCE_DIAGNOSTIC_MAX];
} code_reference_response_t;
/* ABI entry point for module-local qualification tests and the loader. */
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void);
stnlabz_module_result_t code_reference_analyze(
 const void *,size_t,void *,size_t,size_t *,void *);
#endif
