#ifndef DIGIT_S2_PUBLIC_H
#define DIGIT_S2_PUBLIC_H
#include <stddef.h>
#include <time.h>
int s2_public_render(const char *source_json, size_t source_size,
                     size_t tracked, size_t opened, size_t changed,
                     time_t collected_at, char *out, size_t capacity);
int s2_public_publish(const char *report);
#endif
