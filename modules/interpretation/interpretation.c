/*
 * Digit Interpretation -- deterministic lexical observation (ABI 1.5).
 *
 * This service reports linguistic signals and uncertainty. It does not
 * perform full semantic interpretation, fact verification, conversation
 * reasoning, participation decisions, or natural-language generation.
 *
 * No model inference, Ollama, network access, SQL, or scripted replies.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "module.h"
#include "interpretation.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const stnlabz_module_host_t *host_api;

static int in_set(const char *word, const char *const *set, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (!strcmp(word, set[i])) return 1;
    return 0;
}

static int question_initial(const char *word)
{
    static const char *const words[] = {
        "who","what","when","where","why","how","which",
        "is","are","was","were","do","does","did","can",
        "could","would","should","will","have","has","had"
    };
    return in_set(word, words, COUNT(words));
}

static void word_features(const char *word, digit_interpretation_result_t *out)
{
    static const char *const negation[] = {
        "not","no","never","cannot","can't","don't","doesn't",
        "didn't","won't","isn't","wasn't","weren't","aren't"
    };
    static const char *const past[] = {
        "yesterday","earlier","previously","before","ago","last"
    };
    static const char *const future[] = {
        "tomorrow","later","next","upcoming","will","shall"
    };
    static const char *const referents[] = {
        "it","its","this","that","these","those","they",
        "them","their","he","him","his","she","her"
    };
    static const char *const contrast[] = {
        "but","however","although","whereas","nevertheless","instead"
    };
    static const char *const causal[] = {
        "because","therefore","caused","causes","cause","since"
    };
    if (in_set(word, negation, COUNT(negation)))
        out->features |= DIGIT_FEATURE_NEGATION;
    if (in_set(word, past, COUNT(past)))
        out->features |= DIGIT_FEATURE_PAST;
    if (in_set(word, future, COUNT(future)))
        out->features |= DIGIT_FEATURE_FUTURE;
    if (in_set(word, referents, COUNT(referents))) {
        out->features |= DIGIT_FEATURE_REFERENCE;
        ++out->unresolved_references;
    }
    if (in_set(word, contrast, COUNT(contrast)))
        out->features |= DIGIT_FEATURE_CONTRAST;
    if (in_set(word, causal, COUNT(causal)))
        out->features |= DIGIT_FEATURE_CAUSAL_WORD;
}

static void add_word(const char *word, digit_interpretation_result_t *out,
                     int *first_question, int *seen)
{
    size_t n;
    if (!*seen) *first_question = question_initial(word);
    *seen = 1;
    word_features(word, out);
    if (!strcmp(word,"explain") || !strcmp(word,"describe"))
        out->features |= DIGIT_FEATURE_EXPLANATION_REQUEST;
    n = strlen(word);
    if (out->term_count >= DIGIT_INTERPRETATION_TERMS_MAX) {
        out->features |= DIGIT_FEATURE_TRUNCATED;
        return;
    }
    memcpy(out->terms[out->term_count++], word, n + 1);
}

static int interpret(const digit_interpretation_request_t *req,
                     digit_interpretation_result_t *out)
{
    char word[DIGIT_INTERPRETATION_TERM_MAX];
    size_t i, length = 0;
    int question = 0, first_question = 0, seen = 0, overflow = 0;
    if (!req || !out ||
        !memchr(req->text, '\0', sizeof(req->text))) return 0;
    memset(out, 0, sizeof(*out));
    for (i = 0; ; ++i) {
        unsigned char c = (unsigned char)req->text[i];
        int part = (c >= 'A' && c <= 'Z') ||
                   (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') ||
                   c == '_' || c == '\'';
        if (c == '?') question = 1;
        if (part) {
            if (length + 1 < sizeof(word)) {
                word[length++] = (char)tolower(c);
            } else {
                overflow = 1;
                out->features |= DIGIT_FEATURE_TRUNCATED;
            }
        } else if (length || overflow) {
            if (!overflow) {
                word[length] = '\0';
                add_word(word, out, &first_question, &seen);
            }
            length = 0;
            overflow = 0;
        }
        if (!c) break;
    }
    out->kind = !seen ? DIGIT_UTTERANCE_UNRESOLVED :
                question || (first_question && strcmp(out->terms[0],"does") && strcmp(out->terms[0],"do") && strcmp(out->terms[0],"did")) ? DIGIT_UTTERANCE_QUESTION :
                DIGIT_UTTERANCE_STATEMENT;
    return 1;
}

static stnlabz_module_result_t handle(const void *request, size_t request_size,
    void *response, size_t response_size, size_t *used, void *context)
{
    digit_interpretation_result_t result;
    (void)context;
    if (used) *used = 0;
    if (!host_api) return STNLABZ_MODULE_ERR_INVALID_STATE;
    if (!request || request_size != sizeof(digit_interpretation_request_t) ||
        !response || response_size < sizeof(result) || !used)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!interpret(request, &result))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memcpy(response, &result, sizeof(result));
    *used = sizeof(result);
    return STNLABZ_MODULE_OK;
}

static void check(stnlabz_module_qualification_result_t *q, int ok, int negative)
{
    ++q->tests_executed;
    if (ok) ++q->tests_passed;
    else ++q->tests_failed;
    if (negative) {
        q->negative_test_executed = 1;
        if (ok) q->negative_test_passed = 1;
    }
}

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    static const struct {
        const char *text;
        uint32_t kind, required;
    } fixtures[] = {
        {"The database is slow.",1,0},
        {"Why is it slow?",2,DIGIT_FEATURE_REFERENCE},
        {"Did it fail?",2,DIGIT_FEATURE_REFERENCE},
        {"It did not fail.",1,DIGIT_FEATURE_NEGATION},
        {"It failed yesterday.",1,DIGIT_FEATURE_PAST},
        {"It will recover tomorrow.",1,DIGIT_FEATURE_FUTURE},
        {"Failure occurred because of a timeout.",1,DIGIT_FEATURE_CAUSAL_WORD},
        {"The database works but the API fails.",1,DIGIT_FEATURE_CONTRAST},
        {"They changed it.",1,DIGIT_FEATURE_REFERENCE},
        {"What changed?",2,0},
        {"explain php",1,DIGIT_FEATURE_EXPLANATION_REQUEST},
        {"describe databases",1,DIGIT_FEATURE_EXPLANATION_REQUEST},
        {"Could this be incorrect?",2,DIGIT_FEATURE_REFERENCE},
        {"No changes were made.",1,DIGIT_FEATURE_NEGATION},
        {"Before the migration, performance was stable.",1,DIGIT_FEATURE_PAST},
        {"We shall examine the results later.",1,DIGIT_FEATURE_FUTURE},
        {"However, the results differ.",1,DIGIT_FEATURE_CONTRAST},
        {"Who changed the configuration?",2,0},
        {"The cause remains unknown.",1,DIGIT_FEATURE_CAUSAL_WORD},
        {"The server is active.",1,0},
        {"Does it work?",2,DIGIT_FEATURE_REFERENCE},
        {"does paws lickies",1,0},
        {"mrrp",1,0},
        {"puurrs",1,0},
        {"They cannot establish the cause.",1,DIGIT_FEATURE_NEGATION}
    };
    digit_interpretation_request_t req;
    digit_interpretation_result_t out;
    size_t i;
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof(*q));
    for (i = 0; i < COUNT(fixtures); ++i) {
        memset(&req, 0, sizeof(req));
        memcpy(req.text, fixtures[i].text, strlen(fixtures[i].text) + 1);
        check(q, interpret(&req, &out) &&
              out.kind == fixtures[i].kind &&
              (out.features & fixtures[i].required) == fixtures[i].required,
              0);
    }
    memset(&req, 'X', sizeof(req));
    check(q, !interpret(&req, &out), 1);
    memset(&req, 0, sizeof(req));
    check(q, interpret(&req, &out) &&
          out.kind == DIGIT_UTTERANCE_UNRESOLVED, 1);
    check(q, !interpret(NULL, &out), 1);
    check(q, !interpret(&req, NULL), 1);
    return q->tests_executed >= STNLABZ_MODULE_MIN_TESTS &&
           !q->tests_failed && q->negative_test_executed &&
           q->negative_test_passed
        ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_QUALIFICATION;
}

static stnlabz_module_result_t start(const stnlabz_module_host_t *api)
{
    if (host_api || !api || !api->register_service ||
        !api->unregister_service) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!api->register_service(DIGIT_INTERPRETATION_SERVICE, handle, NULL))
        return STNLABZ_MODULE_ERR_START_FAILED;
    host_api = api;
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t stop(void)
{
    if (!host_api) return STNLABZ_MODULE_ERR_INVALID_STATE;
    if (!host_api->unregister_service(DIGIT_INTERPRETATION_SERVICE, NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    host_api = NULL;
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t descriptor = {
    .id = "interpretation",
    .name = "Digit Lexical Interpretation",
    .version_major = 0,
    .version_minor = 1,
    .version_patch = 3,
    .required_core_api_major = STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor = STNLABZ_MODULE_API_MINOR,
    .qualify = qualify,
    .start = start,
    .stop = stop
};

const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
