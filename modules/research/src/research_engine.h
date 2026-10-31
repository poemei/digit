#ifndef DIGIT_RESEARCH_ENGINE_H
#define DIGIT_RESEARCH_ENGINE_H
#include <stddef.h>
typedef struct {
 char title[180];
 char url[480];
 char excerpt[650];
} research_hit_t;
typedef struct {research_hit_t hit[6];unsigned count;} research_hits_t;
void research_runtime_log(const char *event,const char *fmt,...);
unsigned research_parse_web_html(const char *html,research_hits_t *hits);
/* 1: results, 0: provider explicitly reported none, -1: acquisition failure. */
int research_search(const char *query,research_hits_t *hits);
#endif
