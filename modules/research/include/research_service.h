#ifndef DIGIT_RESEARCH_SERVICE_H
#define DIGIT_RESEARCH_SERVICE_H

/* ABI 1.5 service contract: discover through Core, not IRC commands. */
#define DIGIT_RESEARCH_LOOKUP_SERVICE "digit.research.lookup"
#define DIGIT_RESEARCH_TERMS_MAX 512U
#define DIGIT_RESEARCH_RESPONSE_MAX 512U

typedef struct {
    char topic[64];
    char terms[DIGIT_RESEARCH_TERMS_MAX];
} digit_research_request_t;

typedef struct {
    char text[DIGIT_RESEARCH_RESPONSE_MAX];
    unsigned int verified; /* Always 0: source text is NOT approved knowledge. */
} digit_research_response_t;
#endif
