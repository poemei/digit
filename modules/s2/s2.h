#ifndef DIGIT_S2_H
#define DIGIT_S2_H
#define DIGIT_S2_POLL_SERVICE "digit.s2.poll"
#define DIGIT_S2_REPORT_CAP 384
/* Caller submits no request; returns one sanitised one-line report if new evidence
 * exists, else response_used=0. Only the IRC transport sends the report. */
typedef struct { char report[DIGIT_S2_REPORT_CAP]; } digit_s2_report_t;
#endif
