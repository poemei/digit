#ifndef DIGIT_ROUTE_H
#define DIGIT_ROUTE_H

/* Digit Core command-dispatch service. ABI 1.5 host layout is unchanged. */
#define DIGIT_ROUTER_DISPATCH_SERVICE "digit.router.dispatch"
#define DIGIT_ROUTE_REPLY_MAX 512

typedef struct {
    char text[DIGIT_ROUTE_REPLY_MAX];
    int replied;
} digit_route_reply_t;

#endif
