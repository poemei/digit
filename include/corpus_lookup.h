#ifndef DIGIT_CORPUS_LOOKUP_H
#define DIGIT_CORPUS_LOOKUP_H
#define DIGIT_CORPUS_LOOKUP_TERM_MAX 129
#define DIGIT_CORPUS_LOOKUP_TEXT_MAX 4096
typedef struct {
 char term[DIGIT_CORPUS_LOOKUP_TERM_MAX];
} digit_corpus_lookup_request_t;
typedef struct {
 int found; /* 1 record found, 0 no record */
 char id[65];
 char source[256];
 char text[DIGIT_CORPUS_LOOKUP_TEXT_MAX];
} digit_corpus_lookup_response_t;
/* Multi-result read path. Existing corpus.lookup contract remains intact. */
#define DIGIT_CORPUS_MULTI_VERSION 1u
#define DIGIT_CORPUS_MULTI_SERVICE "corpus.lookup.multi"
#define DIGIT_CORPUS_MULTI_MAX 3u
typedef struct {
 unsigned version;
 char term[DIGIT_CORPUS_LOOKUP_TERM_MAX];
} digit_corpus_multi_request_t;
typedef struct {
 unsigned version;
 unsigned count;
 digit_corpus_lookup_response_t items[DIGIT_CORPUS_MULTI_MAX];
} digit_corpus_multi_response_t;
#endif

