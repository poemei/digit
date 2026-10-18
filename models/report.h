#ifndef DIGIT_REPORT_H
#define DIGIT_REPORT_H
#include <stddef.h>
typedef enum {DIGIT_REPORT_UNKNOWN=0,DIGIT_REPORT_IDENTITY,DIGIT_REPORT_MISSION,DIGIT_REPORT_ORDERS,DIGIT_REPORT_FAMILY,DIGIT_REPORT_RULE,DIGIT_REPORT_VERSION} digit_report_kind_t;
digit_report_kind_t digit_report_classify(const char *text);
size_t digit_report_lines(digit_report_kind_t kind);
int digit_report_line(digit_report_kind_t kind,size_t index,char *out,size_t cap);
#endif
