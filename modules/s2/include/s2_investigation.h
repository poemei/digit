#ifndef DIGIT_S2_INVESTIGATION_H
#define DIGIT_S2_INVESTIGATION_H

#include <stddef.h>
#include <time.h>
#include "s2_model.h"

/* Persist bounded case continuity and format source-backed conclusions.
 * The journal stores only Sentinel case IDs, signatures, and collection metadata.
 * No raw IPs, credentials, domains, URLs, or customer data are retained. */
int s2_investigation_update(const char *source_json, size_t source_size,
                            const s2_register_t *register_state,
                            const char *journal_path, time_t collected_at,
                            char *out, size_t capacity);

#endif
