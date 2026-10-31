#ifndef DIGIT_RESEARCH_ENGINE_H
#define DIGIT_RESEARCH_ENGINE_H
#include <stddef.h>
#include "stnlabz_abi/module.h"

/* No network in these helpers: deterministic tests use fixture RSS/HTML. */
typedef struct {
 char title[180];
 char url[480];
 char excerpt[650];
 char date[90];
 unsigned relevance;
} research_hit_t;
typedef struct {
 research_hit_t hit[6];
 unsigned count;
} research_hits_t;

int research_terms(const char *question,char *query,size_t cap);
unsigned research_parse_rss(const char *rss,const char *question,
                            research_hits_t *hits);
int research_html_text(const char *html,char *text,size_t cap);
int research_grounded(const char *answer,const char *evidence);
int research_general_answer(const char *question,const stnlabz_module_host_t *host,
                            char *answer,size_t cap,unsigned *sources);
#endif
