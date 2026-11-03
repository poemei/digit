#ifndef DIGIT_GITHUB_MODULE_H
#define DIGIT_GITHUB_MODULE_H

#include "stnlabz_abi/module.h"

#define DIGIT_GITHUB_REPORT_SERVICE "digit.github.report.v1"
#define DIGIT_GITHUB_REQUEST_MAX 512
#define DIGIT_GITHUB_REPORT_MAX 8192

typedef struct {
    char query[DIGIT_GITHUB_REQUEST_MAX];
} digit_github_request_t;

typedef struct {
    int status; /* 0: success, 1: denied, 2: unavailable, 3: invalid */
    char report[DIGIT_GITHUB_REPORT_MAX];
} digit_github_response_t;

#endif
