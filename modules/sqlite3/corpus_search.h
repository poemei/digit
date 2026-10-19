#ifndef DIGIT_CORPUS_SEARCH_H
#define DIGIT_CORPUS_SEARCH_H
#include <stdint.h>
#define DIGIT_CORPUS_SEARCH_SERVICE "digit.sqlite3.corpus_search"
#define DIGIT_CORPUS_QUERY_MAX 512
#define DIGIT_CORPUS_EXCERPT_MAX 360
#define DIGIT_CORPUS_RESULTS_MAX 3
typedef struct {char query[DIGIT_CORPUS_QUERY_MAX];} digit_corpus_query_t;
typedef struct {int64_t id;char category[48];char verification_status[48];char excerpt[DIGIT_CORPUS_EXCERPT_MAX];} digit_corpus_hit_t;
typedef struct {unsigned count;digit_corpus_hit_t hits[DIGIT_CORPUS_RESULTS_MAX];} digit_corpus_result_t;
#endif
