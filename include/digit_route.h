#ifndef DIGIT_ROUTE_H
#define DIGIT_ROUTE_H

/* Transport-neutral Core ingress. Existing ABI 1.5 host layout stays unchanged. */
#define DIGIT_ROUTER_DISPATCH_SERVICE "digit.router.dispatch"
#define DIGIT_ROUTE_TEXT_MAX 2048
#define DIGIT_ROUTE_REPLY_MAX 512

typedef struct {
    char sender[128];
    char account[128];
    char text[DIGIT_ROUTE_TEXT_MAX];
} digit_route_request_t;

typedef struct {
    char text[DIGIT_ROUTE_REPLY_MAX];
    int replied;
} digit_route_reply_t;

#endif
