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
#include "dsd.h"
#include "../reasoning/reasoning.h"
#include "../sqlite3/corpus_search.h"
#include "response_view.h"

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
    /* Conversation coordinates model retrieval and evidence evaluation.
     * Retrieval terms are candidates, not scripted response triggers.
     * Reasoning cannot claim a fact unless a model provided evidence. */
    {
        digit_reasoning_request_t reasoning={0};
        digit_reasoning_result_t decision={0};
        digit_corpus_query_t search={0};
        digit_corpus_result_t found={0};
        size_t search_used=0,decision_used=0;
        unsigned i;
        reasoning.interpretation=result;
        if(result.term_count){
            /* Single indexed subject term avoids demanding that the entire
             * utterance appear as an exact FTS phrase. No keyword dictionary. */
            for(i=result.term_count;i>0;i--){
                const char *term=result.terms[i-1];
                if(term[0]&&strlen(term)>1){
                    snprintf(search.query,sizeof search.query,"%s",term);
                    break;
                }
            }
        }
        if(search.query[0]){
            reasoning.retrieval_attempted=1;
            rc=host->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&search,
                sizeof search,&found,sizeof found,&search_used);
            reasoning.retrieval_succeeded=(rc==STNLABZ_MODULE_OK&&
                                         search_used==sizeof found&&
                                         found.count<=DIGIT_CORPUS_RESULTS_MAX);
            if(reasoning.retrieval_succeeded)reasoning.corpus=found;
            trace_stage(host,"process","digit.sqlite3.corpus_search",
                reasoning.retrieval_succeeded?"EVIDENCE_RETRIEVED":
                rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"RETRIEVAL_UNAVAILABLE",
                reasoning.retrieval_succeeded?0:2,
                reasoning.retrieval_succeeded?found.count:0,__LINE__);
        }
        rc=host->invoke_service(DIGIT_REASONING_SERVICE,&reasoning,
            sizeof reasoning,&decision,sizeof decision,&decision_used);
        trace_stage(host,"process","digit.reasoning.evaluate",
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?
              (decision.outcome==DIGIT_REASONING_OUTCOME_EVIDENCE?
                "EVIDENCE_AVAILABLE":decision.outcome==DIGIT_REASONING_OUTCOME_CLARIFY?
                "CLARIFICATION_REQUIRED":decision.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN?
                "KNOWLEDGE_UNKNOWN":decision.outcome==DIGIT_REASONING_OUTCOME_FAILURE?
                "RETRIEVAL_FAILED":"EVIDENCE_INSUFFICIENT"):
              rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR",
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?0:1,
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?
                decision.evidence_count:0,__LINE__);
        if(rc!=STNLABZ_MODULE_OK)return rc;
        if(decision_used!=sizeof decision||
           decision.outcome>DIGIT_REASONING_OUTCOME_FAILURE||
           decision.reference_count>DIGIT_REASONING_MAX_REFERENCES)
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        /* A skipped View must be diagnosable, not invisible in DSD. */
        if(strcmp(msg->sender,msg->target)){
            trace_stage(host,"process","response.view","CHANNEL_OBSERVATION",2,0,__LINE__);
        }else if(!response||!capacity){
            trace_stage(host,"process","response.view","OUTPUT_UNAVAILABLE",2,0,__LINE__);
        }else{
            int rendered=digit_response_render(&decision,&found,response,capacity,used);
            const char *status=rendered?
                (decision.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN?
                  "UNKNOWN_STATED":decision.outcome==DIGIT_REASONING_OUTCOME_CLARIFY?
                  "CLARIFICATION_REQUESTED":decision.outcome==DIGIT_REASONING_OUTCOME_FAILURE?
                  "RETRIEVAL_FAILURE_STATED":"RESPONSE_COMPOSED"):
                "EVIDENCE_NOT_RENDERABLE";
            trace_stage(host,"process","response.view",status,
                rendered?0:2,rendered?(unsigned)*used:0,__LINE__);
        }
    }
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
    /* A grounded View response is possible when Reasoning selected
     * complete source evidence; otherwise the response is withheld. */
    trace_stage(host,"process","participation",
      used&&*used?"RESPONSE_RETURNED":"RESPONSE_ABSTAIN",0,
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
    if(!strcmp(service,DIGIT_CORPUS_SEARCH_SERVICE)){
        digit_corpus_result_t empty={0};
        if(used)*used=0;
        if(!request||bytes!=sizeof(digit_corpus_query_t)||!response||
            capacity<sizeof empty||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        memcpy(response,&empty,sizeof empty);*used=sizeof empty;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_REASONING_SERVICE)){
        digit_reasoning_result_t decision={0};
        const digit_reasoning_request_t *req=request;
        if(used)*used=0;
        if(!req||bytes!=sizeof *req||!response||
            capacity<sizeof decision||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        decision.outcome=DIGIT_REASONING_OUTCOME_ABSTAIN;
        memcpy(response,&decision,sizeof decision);*used=sizeof decision;
        return STNLABZ_MODULE_OK;
    }
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

    {
        digit_reasoning_result_t decision={0};
        digit_corpus_result_t data={0};
        char render[512]={0};size_t length=0;
        data.count=1;data.hits[0].id=42;
        strcpy(data.hits[0].verification_status,"operator-verified");
        strcpy(data.hits[0].excerpt,"PHP is a server-side language. Additional text.");
        decision.outcome=DIGIT_REASONING_OUTCOME_EVIDENCE;
        decision.reference_count=1;decision.corpus_ids[0]=42;
        check(q,digit_response_render(&decision,&data,render,sizeof render,&length)
           &&length>0&&strstr(render,"Corpus #42 excerpt: \"PHP is a server-side language.")!=NULL,0);
        decision.requires_verification=1;length=9;
        check(q,!digit_response_render(&decision,&data,render,sizeof render,&length)
           &&length==0,1);
        decision.requires_verification=0;
        strcpy(data.hits[0].verification_status,"verified");
        check(q,digit_response_render(&decision,&data,render,sizeof render,&length)
           &&length>0,0);
        strcpy(data.hits[0].verification_status,"pending");
        check(q,!digit_response_render(&decision,&data,render,sizeof render,&length),1);
        strcpy(data.hits[0].verification_status,"unverified");
        check(q,!digit_response_render(&decision,&data,render,sizeof render,&length),1);
        strcpy(data.hits[0].verification_status,"operator-verified");
        strcpy(data.hits[0].excerpt,"Unfinished evidence fragment");
        check(q,!digit_response_render(&decision,&data,render,sizeof render,&length),1);
        strcpy(data.hits[0].excerpt,"Short fact.");
        decision.corpus_ids[0]=900;
        check(q,!digit_response_render(&decision,&data,render,sizeof render,&length),1);
        decision.corpus_ids[0]=42;
        check(q,!digit_response_render(&decision,&data,render,5,&length),1);
        decision.outcome=DIGIT_REASONING_OUTCOME_UNKNOWN;
        check(q,digit_response_render(&decision,&data,render,sizeof render,&length)
          &&strstr(render,"don't know")!=NULL,0);
        decision.outcome=DIGIT_REASONING_OUTCOME_CLARIFY;
        check(q,digit_response_render(&decision,&data,render,sizeof render,&length)
          &&strstr(render,"clarification")!=NULL,0);
        decision.outcome=DIGIT_REASONING_OUTCOME_FAILURE;
        check(q,digit_response_render(&decision,&data,render,sizeof render,&length)
          &&strstr(render,"retrieval failed")!=NULL,0);

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
    .version_major = 1, .version_minor = 1, .version_patch = 1,
    .required_core_api_major = STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor = STNLABZ_MODULE_API_MINOR,
    .qualify = qualify, .start = start, .stop = stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
