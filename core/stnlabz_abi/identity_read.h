#ifndef DIGIT_IDENTITY_READ_H
#define DIGIT_IDENTITY_READ_H
#include <stdint.h>
#define DIGIT_IDENTITY_READ_SERVICE "digit.core.identity.read"
#define DIGIT_IDENTITY_NAME 1u
#define DIGIT_IDENTITY_LINEAGE 2u
#define DIGIT_IDENTITY_ORGANIZATION 3u
#define DIGIT_IDENTITY_MISSION 4u
#define DIGIT_IDENTITY_MOTHER 5u
typedef struct { uint32_t field; } digit_identity_request_t;
typedef struct { char text[512]; } digit_identity_response_t;
#endif
