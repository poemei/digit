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
/* Optional greeting composition. Identity classification must be supplied
 * by an authorized caller; nicknames alone are not authentication.
 * kind: mommy, auntie, sister, known, unknown.
 * caller decides whether the greeting may be shown.
 */
#define DIGIT_SOCIALS_GREETING_SERVICE "digit.socials.greeting"
#define DIGIT_SOCIALS_GREETING_KIND_MAX 16
#define DIGIT_SOCIALS_GREETING_MAX 128
typedef struct {
 char kind[DIGIT_SOCIALS_GREETING_KIND_MAX];
} digit_socials_greeting_request_t;
typedef struct {
 char text[DIGIT_SOCIALS_GREETING_MAX];
} digit_socials_greeting_response_t;
/* Family interaction is explicitly limited to a trusted account classification
 * supplied by the IRC transport. Ordinary dialogue remains Conversation-owned. */
#define DIGIT_SOCIALS_FAMILY_SERVICE "digit.socials.family"
#define DIGIT_SOCIALS_FAMILY_TEXT_MAX 512
typedef struct {
 char kind[DIGIT_SOCIALS_GREETING_KIND_MAX];
 char text[DIGIT_SOCIALS_FAMILY_TEXT_MAX];
} digit_socials_family_request_t;
typedef digit_socials_greeting_response_t digit_socials_family_response_t;
#endif
