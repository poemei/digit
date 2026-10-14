#ifndef DIGIT_INTERPRETATION_H
#define DIGIT_INTERPRETATION_H
#define DIGIT_INTERPRETATION_VERSION 1u
typedef enum { DIGIT_INTENT_UNKNOWN=0,DIGIT_INTENT_CAPABILITIES=1,DIGIT_INTENT_KNOWLEDGE=2 } digit_intent_t;
typedef struct { unsigned version; char text[513]; } digit_interpret_request_t;
typedef struct { unsigned version; digit_intent_t intent; unsigned confidence; char subject[129]; } digit_interpret_response_t;
#endif
