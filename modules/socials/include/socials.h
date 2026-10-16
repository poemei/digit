#ifndef DIGIT_SOCIALS_H
#define DIGIT_SOCIALS_H
#include <stdint.h>
#define DIGIT_SOCIALS_SERVICE "socials.select"
#define DIGIT_SOCIALS_VERSION 1U
/* Only nonverbal expression categories; no words or dialogue are produced. */
enum digit_socials_category {
 DIGIT_SOCIALS_ANY=0, DIGIT_SOCIALS_HEART=1, DIGIT_SOCIALS_SMILE=2,
 DIGIT_SOCIALS_PLAYFUL=3, DIGIT_SOCIALS_GENTLE=4,
 DIGIT_SOCIALS_CELEBRATE=5
};
typedef struct {
 uint32_t version;
 uint32_t category;
 /* Caller-provided contextual key, not a user identity or a command. */
 char context[128];
} digit_socials_request_t;
typedef struct {
 uint32_t version;
 uint32_t category;
 char emoji[32];
} digit_socials_response_t;
#endif
