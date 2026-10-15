#ifndef DIGIT_CORPUS_LEARN_H
#define DIGIT_CORPUS_LEARN_H
#include <stdint.h>
#define DIGIT_CORPUS_LEARN_SERVICE "corpus.learn.slack"
#define DIGIT_CORPUS_LEARN_VERSION 1u
#define DIGIT_CORPUS_OPS_CHANNEL "C0C9LS4QA1X"
/* Internal module service contract; requires independently checked credential. */
typedef struct {
 uint32_t version;
 char workspace[64];
 char channel[64];
 char user[64];
 char key[65];
 char arguments[512];
} digit_corpus_learn_request_t;
typedef struct {
 uint32_t version;
 char message[160];
} digit_corpus_learn_response_t;
#endif
