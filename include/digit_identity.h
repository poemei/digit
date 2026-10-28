#ifndef DIGIT_IDENTITY_SERVICE_H
#define DIGIT_IDENTITY_SERVICE_H
#include <stddef.h>
#define DIGIT_ROOT_ORDERS_SERVICE "digit.root.orders.read"
#define DIGIT_IDENTITY_ANSWER_SERVICE "digit.identity.answer"
#define DIGIT_IDENTITY_ANSWER_MAX 512
typedef struct { unsigned int index; } digit_root_order_request_t;
typedef struct { char text[256]; } digit_root_order_response_t;
typedef struct { char question[512]; } digit_identity_question_t;
typedef struct { char answer[DIGIT_IDENTITY_ANSWER_MAX]; } digit_identity_answer_t;
#endif
