#ifndef DIGIT_INTEL_REPORT_H
#define DIGIT_INTEL_REPORT_H
#include <stdint.h>
#define DIGIT_INTEL_REPORT_SERVICE "intel.report.read"
#define DIGIT_INTEL_REPORT_VERSION 1u
#define DIGIT_INTEL_REPORT_MAX 4096
typedef struct {uint32_t version;} digit_intel_report_request_t;
typedef struct {uint32_t version;char report[DIGIT_INTEL_REPORT_MAX];} digit_intel_report_response_t;
#endif
