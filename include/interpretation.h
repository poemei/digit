#ifndef DIGIT_INTERPRETATION_H
#define DIGIT_INTERPRETATION_H
#define DIGIT_INTERPRETATION_VERSION 2u
typedef enum { DIGIT_INTENT_UNKNOWN=0,DIGIT_INTENT_CAPABILITIES=1,DIGIT_INTENT_KNOWLEDGE=2,DIGIT_INTENT_CHECKIN=3,DIGIT_INTENT_CORPUS_GUIDANCE=4,DIGIT_INTENT_OBSERVATION=5,DIGIT_INTENT_QUESTION=6,DIGIT_INTENT_REQUEST=7,DIGIT_INTENT_INSTRUCTION=8,DIGIT_INTENT_STATEMENT=9,DIGIT_INTENT_AMBIGUOUS=10,DIGIT_INTENT_GREETING=11 } digit_intent_t;
typedef struct { unsigned version; char text[513]; char corpus_text[4096]; } digit_interpret_request_t;
typedef struct { unsigned version; digit_intent_t intent; unsigned confidence; unsigned addressed; char subject[129]; } digit_interpret_response_t;
#endif
