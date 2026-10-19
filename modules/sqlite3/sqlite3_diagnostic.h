#ifndef DIGIT_SQLITE3_DIAGNOSTIC_H
#define DIGIT_SQLITE3_DIAGNOSTIC_H
#define DIGIT_SQLITE3_DIAGNOSTIC_SERVICE "digit.sqlite3.diagnostic"
/* Fixed-size ABI 1.5 read-only probe output. Schema metadata only.
 * Does not attest that a prior conversational request accessed SQLite3. */
typedef struct {
 char state[32],stage[48],detail[96];
 char source[96],method[48];
 unsigned int line;
} sqlite3_diagnostic_t;
#endif
