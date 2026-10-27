#ifndef DIGIT_S2_MODEL_H
#define DIGIT_S2_MODEL_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char id[96];
    uint64_t hash;
} s2_case_t;
typedef struct {
    s2_case_t *cases;
    size_t count, capacity;
    size_t opened, changed, unchanged, qualifying, missing_severity;
    size_t invalid;
} s2_register_t;
/* Parse an entire Sentinel snapshot, updating the case register in memory.
 * No HTTP, files, Core, or model inference inside the S2 model. */
int s2_ingest_json(s2_register_t *state, const char *body, size_t bytes);
int s2_load_register(s2_register_t *state, const char *path);
int s2_save_register(const s2_register_t *state, const char *path);
void s2_register_free(s2_register_t *state);
int s2_model_selftest(unsigned *executed, unsigned *passed,
                      int *negative_executed, int *negative_passed);
#endif
