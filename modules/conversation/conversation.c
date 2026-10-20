/* Digit Conversation: ABI-only interpretation dispatch; no inference or
 * generated response. Conversation v1.0.4. The IRC module and Core are
 * unchanged; both registered conversation routes share this handler.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "../observation/observation.h"
#include "../sqlite3/context.h"
#include "module.h"
#include "../interpretation/interpretation.h"
#include "../dsd/dsd.h"

#define LISTEN "digit.conversation.listen"
#define DIRECT "digit.conversation"

typedef struct {
    char sender[128], account[128], target[128], message[512];
} message_t;

static const stnlabz_module_host_t *host_api;


/* Structural PHP diagnostic based on observed code boundaries, not on
 * user identity, a keyword-triggered chatbot reply, or invented facts.
 * Require visible HTML, the absence of a PHP opening tag after that HTML,
 * and a complete PHP control-flow statement starting the current message.
 * Insufficient visible source context means abstention. */
static int code_diagnostic(const message_t *msg,
    const digit_observation_recent_result_t *history,
    char *reply, size_t capacity, size_t *used)
{
    size_t i, boundary = DIGIT_OBSERVATION_RECENT_MAX;
    const char *line = msg->message;
    const char *last_html = NULL;
    int opened = 0;
    int length;
    if (!history || history->count > DIGIT_OBSERVATION_RECENT_MAX ||
        !reply || !capacity || !used) return 0;
    while (*line && isspace((unsigned char)*line)) line++;
    /* This is a parser check for PHP control syntax, not an intent classifier. */
    if (strncmp(line, "if", 2) != 0) return 0;
    line += 2;
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '(' || !strchr(line, ')') || strstr(msg->message, "<?php"))
        return 0;
    for (i = 0; i < history->count; ++i) {
        const char *body = history->items[i].text;
        const char *html = strstr(body, "</div>");
        if (html) { boundary = i; last_html = html; opened = 0; }
        if (boundary != DIGIT_OBSERVATION_RECENT_MAX &&
            strstr(body, "<?php") &&
            (!html || strstr(body, "<?php") > html))
            opened = 1;
    }
    if (!last_html || opened) return 0;
    /* The output is a source-grounded diagnostic: never silently claim
     * to have inspected or repaired a module. */
    length = snprintf(reply, capacity,
        "%s: The if statement follows an HTML closing tag without a PHP "
        "opening tag in the shown code. Add <?php before that PHP statement.",
        msg->sender);
    if (length <= 0 || (size_t)length >= capacity) return 0;
    *used = (size_t)length;
    return 1;
}

/* Request-correlated evidence. Inert when no DSD trace is bound. */
static void trace_stage(const stnlabz_module_host_t *api,
 const char *method,const char *operation,const char *status,
 unsigned result,unsigned records,unsigned line){
 uint64_t trace=0,token=1;size_t used=0;
 digit_dsd_record_t r={0};
 if(!api||!api->invoke_service)return;
 if(api->invoke_service(DIGIT_DSD_CURRENT_SERVICE,&token,sizeof token,
   &trace,sizeof trace,&used)!=STNLABZ_MODULE_OK||
   used!=sizeof trace||!trace)return;
 r.trace_id=r.event.trace_id=trace;
 r.event.span_id=3; r.event.parent_span_id=1;
 r.event.line=line;r.event.result=(unsigned char)result;
 r.event.records=records;
 snprintf(r.event.module,sizeof r.event.module,"conversation");
 snprintf(r.event.method,sizeof r.event.method,"%s",method);
 snprintf(r.event.file,sizeof r.event.file,"modules/conversation/conversation.c");
 snprintf(r.event.operation,sizeof r.event.operation,"%s",operation);
 snprintf(r.event.status,sizeof r.event.status,"%s",status);
 (void)api->invoke_service(DIGIT_DSD_RECORD_SERVICE,&r,sizeof r,NULL,0,&used);
}
static stnlabz_module_result_t process(const stnlabz_module_host_t *host,
    const void *request, size_t bytes, void *response, size_t capacity,
    size_t *used)
{
    const message_t *msg = request;
    digit_interpretation_request_t query = {{0}};
    digit_interpretation_result_t result = {0};
    size_t returned = 0;
    stnlabz_module_result_t rc;
    if (used) *used = 0;
    if (!request || bytes != sizeof(message_t) || !used)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!memchr(msg->sender, 0, sizeof msg->sender) ||
        !memchr(msg->account, 0, sizeof msg->account) ||
        !memchr(msg->target, 0, sizeof msg->target) ||
        !memchr(msg->message, 0, sizeof msg->message))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!host || !host->invoke_service)
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    memcpy(query.text, msg->message, sizeof query.text);
    rc = host->invoke_service(DIGIT_INTERPRETATION_SERVICE, &query,
        sizeof query, &result, sizeof result, &returned);
    trace_stage(host,"process","digit.interpretation",
      rc==STNLABZ_MODULE_OK?"SERVICE_OK":
      rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR",
      rc==STNLABZ_MODULE_OK?0:1,(unsigned)returned,__LINE__);
    if (rc != STNLABZ_MODULE_OK) return rc;
    if (returned != sizeof result ||
        result.kind > DIGIT_UTTERANCE_QUESTION ||
        result.term_count > DIGIT_INTERPRETATION_TERMS_MAX ||
        result.unresolved_references > DIGIT_INTERPRETATION_TERMS_MAX)
        return STNLABZ_MODULE_ERR_INVALID_STATE;
    /* Observation persistence and context creation are atomic in SQLite3.
     * Conversation reads that context; it never duplicates observation writes.
     * A nonempty IRC account remains required for this channel route. */
    if (!strcmp(msg->target, "#stn-labz") && msg->account[0]) {
        digit_context_recent_query_t lookup = {0};
        digit_context_recent_result_t history = {0};
        size_t seen = 0;
        memcpy(lookup.channel, msg->target, sizeof lookup.channel);
        seen = 0;
        rc = host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE, &lookup,
            sizeof lookup, &history, sizeof history, &seen);
        trace_stage(host,"process","digit.sqlite3.context_recent",
          rc==STNLABZ_MODULE_OK?"SERVICE_OK":
          rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR",
          rc==STNLABZ_MODULE_OK?0:1,rc==STNLABZ_MODULE_OK?history.count:0,__LINE__);
        if (rc != STNLABZ_MODULE_OK) return rc;
        if (seen != sizeof history || history.count > DIGIT_CONTEXT_MAX)
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        if (response && capacity) {
            digit_observation_recent_result_t visible = {0};
            uint32_t i, first = history.count > DIGIT_OBSERVATION_RECENT_MAX ?
                history.count - DIGIT_OBSERVATION_RECENT_MAX : 0;
            for (i = first; i < history.count; ++i) {
                const digit_context_item_t *item = &history.items[i];
                if (!memchr(item->value, 0, sizeof item->value))
                    return STNLABZ_MODULE_ERR_INVALID_STATE;
                memcpy(visible.items[visible.count].text, item->value,
                    sizeof visible.items[visible.count].text);
                ++visible.count;
            }
            (void)code_diagnostic(msg, &visible, response, capacity, used);
        }
    }
    /* The current Conversation implementation has no general response
     * generator or autonomous participation decision. The only positive
     * response path above is the narrow code diagnostic. Report the actual
     * capability boundary rather than a generic transport silence. */
    trace_stage(host,"process","participation",
      used&&*used?"RESPONSE_RETURNED":"NO_GENERATOR",0,
      used?(unsigned)*used:0,__LINE__);
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t handle(const void *request, size_t bytes,
    void *response, size_t capacity, size_t *used, void *context)
{
    (void)context;
    return process(host_api, request, bytes, response, capacity, used);
}

/* Qualification exercises dispatch with a deterministic ABI stub. It does
 * not claim to test the real Interpretation module or live IRC integration. */
static int mock_calls;
static int mock_mode;
static int mock_context_writes, mock_context_reads;
static stnlabz_module_result_t mock_invoke(const char *service,
    const void *request, size_t bytes, void *response, size_t capacity,
    size_t *used)
{
    const digit_interpretation_request_t *q = request;
    digit_interpretation_result_t r = {0};
    if(!strcmp(service,DIGIT_DSD_CURRENT_SERVICE))return STNLABZ_MODULE_ERR_NOT_FOUND;
    if (!strcmp(service, DIGIT_CONTEXT_STORE_SERVICE)) {
        const digit_context_store_request_t *s=request;
        if (!s || bytes!=sizeof *s || strcmp(s->channel,"#stn-labz") ||
            !s->sender[0] || !s->source_account[0] || !s->text[0] ||
            (s->kind!=1 && s->kind!=2))
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        ++mock_context_writes;
        if(used)*used=0;
        return STNLABZ_MODULE_OK;
    }
    if (!strcmp(service, DIGIT_CONTEXT_RECENT_SERVICE)) {
        const digit_context_recent_query_t *q=request;
        digit_context_recent_result_t h={0};
        if(!q || bytes!=sizeof *q || strcmp(q->channel,"#stn-labz") ||
           !response || capacity<sizeof h || !used)
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        ++mock_context_reads;
        memcpy(response,&h,sizeof h);
        *used=sizeof h;
        return STNLABZ_MODULE_OK;
    }
    mock_calls++;
    if (strcmp(service, DIGIT_INTERPRETATION_SERVICE) ||
        !request || bytes != sizeof *q ||
        !response || capacity < sizeof r || !used ||
        !memchr(q->text, 0, sizeof q->text))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (mock_mode == 1) return STNLABZ_MODULE_ERR_NOT_FOUND;
    if (mock_mode == 2) return STNLABZ_MODULE_ERR_INVALID_STATE;
    r.kind = DIGIT_UTTERANCE_STATEMENT;
    if (mock_mode == 4) r.kind = 99;
    if (mock_mode == 5) r.term_count = DIGIT_INTERPRETATION_TERMS_MAX + 1;
    if (mock_mode == 6)
        r.unresolved_references = DIGIT_INTERPRETATION_TERMS_MAX + 1;
    memcpy(response, &r, sizeof r);
    *used = mock_mode == 3 ? sizeof r - 1 : sizeof r;
    return STNLABZ_MODULE_OK;
}

static void check(stnlabz_module_qualification_result_t *q,
    int good, int negative)
{
    ++q->tests_executed;
    if (good) ++q->tests_passed;
    else ++q->tests_failed;
    if (negative) {
        q->negative_test_executed = 1;
        if (good) q->negative_test_passed = 1;
    }
}

static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    const stnlabz_module_host_t mock = {.invoke_service = mock_invoke};
    const stnlabz_module_host_t missing = {0};
    message_t msg = {0}, invalid;
    char response[512] = {0};
    size_t used;
    stnlabz_module_result_t rc;
    int before, mode;
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof *q);
    memcpy(msg.sender, "Alice", 6);
    memcpy(msg.account, "alice", 6);
    memcpy(msg.target, "#test", 6);
    memcpy(msg.message, "Database latency increased", 27);
    mock_mode = 0; mock_calls = 0;

    used = 8;
    rc = process(&mock, &msg, sizeof msg, response, sizeof response, &used);
    check(q, rc == STNLABZ_MODULE_OK && !used && mock_calls == 1, 0);
    check(q, !strcmp(msg.message, "Database latency increased"), 0);
    check(q, !strcmp(msg.sender, "Alice"), 0);
    check(q, !strcmp(msg.account, "alice"), 0);
    check(q, !strcmp(msg.target, "#test"), 0);

    for (mode = 1; mode <= 6; ++mode) {
        stnlabz_module_result_t expected =
            mode == 1 ? STNLABZ_MODULE_ERR_NOT_FOUND :
            mode == 2 ? STNLABZ_MODULE_ERR_INVALID_STATE :
            STNLABZ_MODULE_ERR_INVALID_STATE;
        mock_mode = mode; used = 10;
        rc = process(&mock, &msg, sizeof msg, response, sizeof response, &used);
        check(q, rc == expected && used == 0, 1);
    }
    mock_mode = 0;
    used = 17; before = mock_calls;
    check(q, process(&mock, NULL, sizeof msg, response, sizeof response, &used)
          == STNLABZ_MODULE_ERR_INVALID_ARGUMENT && used == 0 &&
          mock_calls == before, 1);
    used = 17;
    check(q, process(&mock, &msg, sizeof msg - 1, response, sizeof response,
          &used) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT && !used, 1);
    check(q, process(&mock, &msg, sizeof msg, response, sizeof response,
          NULL) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT, 1);
    used = 17;
    check(q, process(NULL, &msg, sizeof msg, response, sizeof response, &used)
          == STNLABZ_MODULE_ERR_INVALID_STATE && !used, 1);
    used = 17;
    check(q, process(&missing, &msg, sizeof msg, response, sizeof response,
          &used) == STNLABZ_MODULE_ERR_INVALID_STATE && !used, 1);
    invalid = msg; memset(invalid.sender, 'x', sizeof invalid.sender);
    used = 17;
    check(q, process(&mock, &invalid, sizeof invalid, response,
          sizeof response, &used) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT &&
          !used, 1);
    invalid = msg; memset(invalid.account, 'x', sizeof invalid.account);
    used = 17;
    check(q, process(&mock, &invalid, sizeof invalid, response,
          sizeof response, &used) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT &&
          !used, 1);
    invalid = msg; memset(invalid.target, 'x', sizeof invalid.target);
    used = 17;
    check(q, process(&mock, &invalid, sizeof invalid, response,
          sizeof response, &used) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT &&
          !used, 1);
    invalid = msg; memset(invalid.message, 'x', sizeof invalid.message);
    used = 17;
    check(q, process(&mock, &invalid, sizeof invalid, response,
          sizeof response, &used) == STNLABZ_MODULE_ERR_INVALID_ARGUMENT &&
          !used, 1);
    used = 17;
    check(q, process(&mock, &msg, sizeof msg, NULL, 0, &used)
          == STNLABZ_MODULE_OK && !used, 0);
    check(q, mock_calls == before + 1, 0);
    check(q, sizeof(msg.message) == sizeof(((digit_interpretation_request_t *)0)->text), 0);
    check(q, strcmp(LISTEN, DIRECT) != 0, 0);
    {
        message_t channel=msg;
        int reads=mock_context_reads;
        strcpy(channel.target,"#stn-labz");
        used=44;
        check(q,process(&mock,&channel,sizeof channel,response,
              sizeof response,&used)==STNLABZ_MODULE_OK &&
              used==0 && mock_context_reads==reads+1,0);
        strcpy(channel.account,"");
        reads=mock_context_reads;
        check(q,process(&mock,&channel,sizeof channel,response,
              sizeof response,&used)==STNLABZ_MODULE_OK &&
              mock_context_reads==reads,1);
    }

    {
        digit_observation_recent_result_t history = {0};
        size_t emitted = 0;
        char output[512] = {0};
        message_t sample = {0};
        strcpy(sample.sender, "Laurie");
        strcpy(sample.target, "#stn-labz");
        strcpy(sample.message, "if (!theme::render('foot', get_defined_vars())) {");
        history.count = 1;
        strcpy(history.items[0].text, "</div>");
        check(q, code_diagnostic(&sample, &history, output,
              sizeof output, &emitted) && emitted > 0 &&
              strstr(output, "<?php") != NULL, 0);
        strcpy(history.items[0].text, "<?php");
        emitted = 0;
        check(q, !code_diagnostic(&sample, &history, output,
              sizeof output, &emitted) && !emitted, 1);
        strcpy(history.items[0].text, "</div>");
        strcpy(sample.message, "Hello everyone");
        check(q, !code_diagnostic(&sample, &history, output,
              sizeof output, &emitted), 1);
        strcpy(sample.message, "<?php if (true) {");
        check(q, !code_diagnostic(&sample, &history, output,
              sizeof output, &emitted), 1);
        strcpy(sample.message, "if (true) {");
        check(q, !code_diagnostic(&sample, &history, output,
              8, &emitted), 1);
    }

    return q->tests_executed >= STNLABZ_MODULE_MIN_TESTS &&
        !q->tests_failed && q->negative_test_executed &&
        q->negative_test_passed ? STNLABZ_MODULE_OK :
        STNLABZ_MODULE_ERR_QUALIFICATION;
}

static stnlabz_module_result_t start(const stnlabz_module_host_t *api)
{
    if (host_api || !api || !api->invoke_service ||
        !api->register_service || !api->unregister_service)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (!api->register_service(LISTEN, handle, NULL))
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (!api->register_service(DIRECT, handle, NULL)) {
        api->unregister_service(LISTEN, NULL);
        return STNLABZ_MODULE_ERR_START_FAILED;
    }
    host_api = api;
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t stop(void)
{
    if (!host_api) return STNLABZ_MODULE_ERR_INVALID_STATE;
    if (!host_api->unregister_service(DIRECT, NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    if (!host_api->unregister_service(LISTEN, NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    host_api = NULL;
    return STNLABZ_MODULE_OK;
}

static const stnlabz_module_descriptor_t descriptor = {
    .id = "conversation",
    .name = "Conversation Interpretation Dispatch",
    .version_major = 1, .version_minor = 0, .version_patch = 6,
    .required_core_api_major = STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor = STNLABZ_MODULE_API_MINOR,
    .qualify = qualify, .start = start, .stop = stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
