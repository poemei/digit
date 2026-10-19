#ifndef DIGIT_SOCIALS_H
#define DIGIT_SOCIALS_H
/* ABI service digit.socials.select. Callers decide intent and whether
 * social expression is appropriate; module only selects one token.
 * Request and response are bytewise stable, zero-padded structures.
 */
#define DIGIT_SOCIALS_SERVICE "digit.socials.select"
#define DIGIT_SOCIALS_CATEGORY_MAX 24
#define DIGIT_SOCIALS_EXPRESSION_MAX 32
typedef struct {
 char category[DIGIT_SOCIALS_CATEGORY_MAX]; /* NUL-terminated category */
} digit_socials_request_t;
typedef struct {
 char expression[DIGIT_SOCIALS_EXPRESSION_MAX]; /* NUL-terminated UTF-8 */
} digit_socials_response_t;
#endif
