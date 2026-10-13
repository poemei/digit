#ifndef DIGIT_CORPUS_STORE_H
#define DIGIT_CORPUS_STORE_H
#include <stddef.h>
#define DIGIT_CORPUS_PATH "/opt/digit/corpus/corpus.tsv"
#define DIGIT_CORPUS_ID_MAX 65
#define DIGIT_CORPUS_CATEGORY_MAX 64
#define DIGIT_CORPUS_SOURCE_MAX 256
#define DIGIT_CORPUS_TEXT_MAX 4096
typedef struct {
 char id[DIGIT_CORPUS_ID_MAX];
 char category[DIGIT_CORPUS_CATEGORY_MAX];
 char source[DIGIT_CORPUS_SOURCE_MAX];
 char text[DIGIT_CORPUS_TEXT_MAX];
} digit_corpus_record_t;
/* 1 found, 0 absent, -1 inaccessible, -2 malformed query. No writes. */
int digit_corpus_find(const char *path,const char *term,digit_corpus_record_t *out);
#endif
