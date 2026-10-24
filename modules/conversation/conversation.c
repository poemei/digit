/* Digit Conversation: ABI-only interpretation dispatch; no inference or
 * generated response. Conversation v1.2.7. The IRC module and Core are
 * unchanged; both registered conversation routes share this handler.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include "../observation/observation.h"
#include "../sqlite3/context.h"
#include "module.h"
#include "identity_read.h"
#include "../interpretation/interpretation.h"
#include "dsd.h"
#include "../reasoning/reasoning.h"
#include "../sqlite3/corpus_search.h"
#include "response_view.h"
#include "../socials/socials.h"
#include "../social_intelligence/social_intelligence.h"
/* [AI:GPT-6 | 2026-10-20 UTC] */
#include "../gru_learning/gru_learning.h"
#include "../gru_bridge/gru_bridge.h"
/* [End AI:GPT-6] */

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
/* Identity questions are answered from Core, never from Corpus guesses. */
static int has_term(const digit_interpretation_result_t *r,const char *term){
 unsigned i;
 for(i=0;i<r->term_count;i++)if(!strcmp(r->terms[i],term))return 1;
 return 0;
}
static int core_identity_field(const digit_interpretation_result_t *r,
    uint32_t *field,int *creator){
 int self=has_term(r,"you")||has_term(r,"your")||
          has_term(r,"digit")||has_term(r,"digicat");
 if(!self)return 0;
 *creator=0;
 if(has_term(r,"mission")){*field=DIGIT_IDENTITY_MISSION;return 1;}
 if(has_term(r,"creator")||has_term(r,"created")){
  *field=DIGIT_IDENTITY_MOTHER;*creator=1;return 1;
 }
 if(has_term(r,"mother")){*field=DIGIT_IDENTITY_MOTHER;return 1;}
 if(has_term(r,"organization")){*field=DIGIT_IDENTITY_ORGANIZATION;return 1;}
 /* Broad what/who questions about work, knowledge or capabilities
  * are not Core lineage questions. */
 if((has_term(r,"what")||has_term(r,"who"))&&
    (has_term(r,"is")||has_term(r,"are"))&&
    !has_term(r,"do")&&!has_term(r,"did")&&!has_term(r,"doing")&&
    !has_term(r,"working")&&!has_term(r,"today")&&
    !has_term(r,"know")&&!has_term(r,"explain")){
  *field=DIGIT_IDENTITY_LINEAGE;return 1;
 }
 return 0;
}
/* Conversational participation is decided here, after Interpretation.
 * A question mark alone is not an invitation. */
static int conversation_invitation(const digit_interpretation_result_t *r){
 static const char *const openings[]={
   "who","what","what's","whats","when","where","why","how",
   "which","can","could","would","will","do","does","did",
   "is","are","should","tell","describe","explain"
 };
 unsigned i,j;
 if(r->kind!=DIGIT_UTTERANCE_QUESTION&&
    !(r->features&DIGIT_FEATURE_EXPLANATION_REQUEST))return 0;
 for(i=0;i<r->term_count&&i<4;i++){
    const char *term=r->terms[i];
    for(j=0;j<sizeof openings/sizeof openings[0];j++)
       if(!strcmp(term,openings[j]))return 1;
 }
 return 0;
}
/* Conversation sends interpreted requests to the registered GRU language
 * generator. Model output is not evidence. For supported factual answers,
 * the evidence-backed View remains the fallback, and generated wording must
 * retain the verified quotation rather than silently replacing it. */
static int gru_generate_response(const stnlabz_module_host_t *api,
 const char *message,const char *verified,const char *source_quote,
 char *output,size_t capacity,size_t *used){
 digit_gru_generate_request_t request={0};
 digit_gru_generate_result_t result={0};
 size_t received=0;stnlabz_module_result_t rc;
 int n;
 if(!api||!api->invoke_service||!message||!message[0]||
    !output||capacity<2||!used)return 0;
 n=snprintf(request.prompt,sizeof request.prompt,
    "Respond naturally to the user's message. "
    "Only assert factual claims supported by the verified evidence. "
    "If the evidence does not establish an answer, express uncertainty. "
    "Do not invent events, actions, knowledge, or sources. "
    "Do not expose internal Corpus IDs, retrieval labels, or diagnostic metadata. "
    "User message: %s\nVerified evidence: %s",
    message,verified&&*verified?verified:"(none)");
 if(n<0||(size_t)n>=sizeof request.prompt)return 0;
 request.length=96;
 rc=api->invoke_service(DIGIT_GRU_GENERATE_SERVICE,&request,sizeof request,
    &result,sizeof result,&received);
 if(rc!=STNLABZ_MODULE_OK||received!=sizeof result||
    !memchr(result.text,0,sizeof result.text))return 0;
 size_t length=strlen(result.text);
 if(!length||length>=capacity||length!=result.generated_bytes)return 0;
 for(size_t i=0;i<length;i++)
    if((unsigned char)result.text[i]<32||
       (unsigned char)result.text[i]==127)return 0;
 if(source_quote&&*source_quote&&!strstr(result.text,source_quote))return 0;
 if(strstr(result.text,"Corpus #")||strstr(result.text,"corpus #")||
    strstr(result.text,"source_record_id")||
    strstr(result.text,"verification_status"))return 0;
 memcpy(output,result.text,length+1);
 *used=length;
 return 1;
}
static stnlabz_module_result_t process_route(const stnlabz_module_host_t *host,
    const void *request, size_t bytes, void *response, size_t capacity,
    size_t *used, int passive)
{
    const message_t *msg = request;
    digit_interpretation_request_t query = {{0}};
    digit_interpretation_result_t result = {0};
    size_t returned = 0;
    stnlabz_module_result_t rc;
    int participate;
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
    /* Context is an input to evidence selection, not an afterthought.
     * Keep one read per channel message and reuse it for diagnostics. */
    digit_context_recent_result_t history={0};
    int have_history=0;
    if(!strcmp(msg->target,"#stn-labz")&&msg->account[0]){
        digit_context_recent_query_t lookup={0};
        size_t seen=0;
        memcpy(lookup.channel,msg->target,sizeof lookup.channel);
        rc=host->invoke_service(DIGIT_CONTEXT_RECENT_SERVICE,&lookup,
            sizeof lookup,&history,sizeof history,&seen);
        trace_stage(host,"process",DIGIT_CONTEXT_RECENT_SERVICE,
            rc==STNLABZ_MODULE_OK?"SERVICE_OK":
            rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR",
            rc==STNLABZ_MODULE_OK?0:1,
            rc==STNLABZ_MODULE_OK?history.count:0,__LINE__);
        if(rc!=STNLABZ_MODULE_OK)return rc;
        if(seen!=sizeof history||history.count>DIGIT_CONTEXT_MAX)
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        have_history=1;
    }
    participate=!passive||conversation_invitation(&result);
    /* Social-specific interpretation and participation are delegated.
     * Missing optional service is not a factual query. */
    {
        digit_social_intelligence_request_t social={0};
        digit_social_intelligence_result_t social_result={0};
        size_t social_used=0;
        snprintf(social.channel,sizeof social.channel,"%s",msg->target);
        snprintf(social.account,sizeof social.account,"%s",msg->account);
        snprintf(social.text,sizeof social.text,"%s",msg->message);
        social.interpretation=result;social.passive=(uint32_t)passive;
        rc=host->invoke_service(DIGIT_SOCIAL_INTELLIGENCE_SERVICE,
            &social,sizeof social,&social_result,sizeof social_result,&social_used);
        trace_stage(host,"process",DIGIT_SOCIAL_INTELLIGENCE_SERVICE,
           rc==STNLABZ_MODULE_OK?"SERVICE_OK":
           rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_UNAVAILABLE":"SERVICE_ERROR",
           rc==STNLABZ_MODULE_OK?0:1,
           rc==STNLABZ_MODULE_OK?(unsigned)social_used:0,__LINE__);
        if(rc==STNLABZ_MODULE_OK&&
           (social_used!=sizeof social_result||social_result.handled>1||
            !memchr(social_result.response,0,sizeof social_result.response)))
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        if(rc==STNLABZ_MODULE_OK&&social_result.handled){
            if(response&&capacity&&social_result.response[0]){
                int n=snprintf(response,capacity,"%s",social_result.response);
                if(n<=0||(size_t)n>=capacity)
                    return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
                *used=(size_t)n;
            }
            return STNLABZ_MODULE_OK;
        }
        if(rc!=STNLABZ_MODULE_OK&&rc!=STNLABZ_MODULE_ERR_NOT_FOUND)
            return rc;
    }
    if(!passive&&(!msg->message[0]||
       (result.kind==DIGIT_UTTERANCE_UNRESOLVED&&
        !strcmp(msg->message,"?")))){
        int n;
        if(!response||!capacity)return STNLABZ_MODULE_OK;
        n=snprintf(response,capacity,"I'm here. What's on your mind?");
        if(n<=0||(size_t)n>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        *used=(size_t)n;
        trace_stage(host,"process","participation","CONTACT_ACKNOWLEDGED",
            0,(unsigned)*used,__LINE__);
        return STNLABZ_MODULE_OK;
    }
    trace_stage(host,"process","participation",
       participate?"ENGAGEMENT_SELECTED":"PASSIVE_OBSERVATION",
       participate?0:2,0,__LINE__);
    {
        uint32_t field=0;
        int creator=0;
        if(participate&&response&&capacity&&
           core_identity_field(&result,&field,&creator)){
            digit_identity_request_t identity={field};
            digit_identity_response_t core_value={{0}};
            size_t value_used=0;
            int written;
            rc=host->invoke_service(DIGIT_IDENTITY_READ_SERVICE,
                 &identity,sizeof identity,&core_value,sizeof core_value,
                 &value_used);
            trace_stage(host,"process",DIGIT_IDENTITY_READ_SERVICE,
               rc==STNLABZ_MODULE_OK?"CORE_IDENTITY_READ":"CORE_IDENTITY_UNAVAILABLE",
               rc==STNLABZ_MODULE_OK?0:1,0,__LINE__);
            if(rc==STNLABZ_MODULE_OK&&value_used==sizeof core_value&&
                memchr(core_value.text,0,sizeof core_value.text)){
                if(creator)
                    written=snprintf(response,capacity,
                       "Core identifies %s as my mother, but does not specify my creator.",
                       core_value.text);
                else if(field==DIGIT_IDENTITY_MISSION)
                    written=snprintf(response,capacity,"%s",core_value.text);
                else if(field==DIGIT_IDENTITY_LINEAGE)
                    written=snprintf(response,capacity,
                       "I am Digit, a %s within STN-LABZ.",core_value.text);
                else
                    written=snprintf(response,capacity,"%s",core_value.text);
                if(written>0&&(size_t)written<capacity){
                    *used=(size_t)written;
                    trace_stage(host,"process","participation",
                       "CORE_IDENTITY_RESPONSE",0,(unsigned)*used,__LINE__);
                    return STNLABZ_MODULE_OK;
                }
                return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
            }
            /* Missing service is a technical defect, not missing knowledge. */
            written=snprintf(response,capacity,
               "My Core identity is unavailable to this conversation.");
            if(written>0&&(size_t)written<capacity){
               *used=(size_t)written;return STNLABZ_MODULE_OK;
            }
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        }
    }
    /* Conversation coordinates model retrieval and evidence evaluation.
     * Retrieval terms are candidates, not scripted response triggers.
     * Reasoning cannot claim a fact unless a model provided evidence. */
    /* [AI:GPT-6 | 2026-10-21 UTC]
     * Imperative information requests ("tell me about PHP") are inquiries
     * even if Interpretation does not classify them as a question. */
    if(result.kind==DIGIT_UTTERANCE_QUESTION ||
       (result.features&DIGIT_FEATURE_EXPLANATION_REQUEST) ||
       !strncasecmp(msg->message,"tell me about ",14) ||
       !strncasecmp(msg->message,"explain ",8) ||
       !strncasecmp(msg->message,"describe ",9)){ /* [End AI:GPT-6] */
        digit_reasoning_request_t reasoning={0};
        digit_reasoning_result_t decision={0};
        digit_corpus_query_t search={0};
        digit_corpus_result_t found={0};
        size_t decision_used=0;
        unsigned i;
        reasoning.interpretation=result;
        /* Evaluate all informative candidate searches before choosing a
         * retrieval set. FTS rank within a single word is not evidence
         * relevance to the complete question. Keep only verified records
         * that overlap more of the question's content-bearing subjects. */
        {
            static const char *const grammatical[]={
                "a","an","the","do","does","did","is","are","was","were",
                "she","he","they","them","her","him","it","this","that",
                "you","your","my","our","who","what","where","when",
                "why","how","can","could","would","should","will",
                "about","of","to","and","or","in","on",
                "me","myself","yourself","ourselves","themselves",
                "tell","explain","describe","please","know","think",
                "say","said","mean","means","talk","answer","question",
                "long","much","many","take","takes","taking","time",
                "today","tomorrow","now","then","before","after",
                "here","there","doing","done","just","really"
            };
            unsigned best_score=0;
            for(i=0;i<result.term_count;i++){
                const char *term=result.terms[i];
                unsigned j,skip=0;
                digit_corpus_result_t candidate={0};
                size_t candidate_used=0;
                if(strlen(term)<3)continue;
                for(j=0;j<sizeof grammatical/sizeof grammatical[0];j++)
                    if(!strcmp(term,grammatical[j])){skip=1;break;}
                if(skip)continue;
                memset(&search,0,sizeof search);
                snprintf(search.query,sizeof search.query,"%s",term);
                reasoning.retrieval_attempted=1;
                rc=host->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,&search,
                    sizeof search,&candidate,sizeof candidate,&candidate_used);
                if(rc!=STNLABZ_MODULE_OK||candidate_used!=sizeof candidate||
                   candidate.count>DIGIT_CORPUS_RESULTS_MAX)continue;
                reasoning.retrieval_succeeded=1;
                for(j=0;j<candidate.count;j++){
                    const digit_corpus_hit_t *hit=&candidate.hits[j];
                    unsigned k,score=0;
                    if(strcmp(hit->verification_status,"verified")&&
                       strcmp(hit->verification_status,"operator-verified"))
                        continue;
                    if(!memchr(hit->excerpt,0,sizeof hit->excerpt))continue;
                    for(k=0;k<result.term_count;k++){
                        const char *word=result.terms[k];
                        size_t n=strlen(word),pos;
                        unsigned stop=0,m;
                        if(n<3)continue;
                        for(m=0;m<sizeof grammatical/sizeof grammatical[0];m++)
                            if(!strcmp(word,grammatical[m])){stop=1;break;}
                        if(stop)continue;
                        for(pos=0;hit->excerpt[pos];pos++){
                            size_t m=0;
                            if(pos&&isalnum((unsigned char)hit->excerpt[pos-1]))
                                continue;
                            while(m<n&&hit->excerpt[pos+m]&&
                                  tolower((unsigned char)hit->excerpt[pos+m])==
                                  (unsigned char)word[m])m++;
                            if(m==n&&!isalnum((unsigned char)hit->excerpt[pos+m])){
                                score++;break;
                            }
                        }
                    }
                    if(score>best_score){
                        memset(&found,0,sizeof found);
                        found.count=1;found.hits[0]=*hit;
                        best_score=score;
                    }
                }
            }
            if(found.count)reasoning.corpus=found;
        }
        /* If the current referential question yielded no evidence, use
         * recently observed discourse as additional search candidates.
         * This does not equate any pronoun to an individual: previous
         * utterances remain provenance-bound observations, not facts. */
        if(!found.count&&have_history&&
           (result.features&DIGIT_FEATURE_REFERENCE)&&
           result.unresolved_references>0){
            unsigned h;
            for(h=history.count;h>0&&!found.count;h--){
                digit_interpretation_request_t prior={0};
                digit_interpretation_result_t parsed={0};
                const digit_context_item_t *item=&history.items[h-1];
                size_t parsed_size=0;
                unsigned t;
                if(strcmp(item->context_type,"utterance.observed")||
                   !memchr(item->value,0,sizeof item->value)||
                   !strcmp(item->value,msg->message))continue;
                snprintf(prior.text,sizeof prior.text,"%s",item->value);
                rc=host->invoke_service(DIGIT_INTERPRETATION_SERVICE,
                    &prior,sizeof prior,&parsed,sizeof parsed,&parsed_size);
                if(rc!=STNLABZ_MODULE_OK||parsed_size!=sizeof parsed||
                   parsed.term_count>DIGIT_INTERPRETATION_TERMS_MAX)continue;
                for(t=parsed.term_count;t>0;t--){
                    const char *term=parsed.terms[t-1];
                    digit_corpus_result_t candidate={0};
                    size_t candidate_used=0;
                    if(strlen(term)<4)continue;
                    /* No identity or pronoun assignment from recency. */
                    if(!strcmp(term,"they")||!strcmp(term,"them")||
                       !strcmp(term,"this")||!strcmp(term,"that")||
                       !strcmp(term,"what")||!strcmp(term,"does")||
                       !strcmp(term,"have")||!strcmp(term,"were"))continue;
                    memset(&search,0,sizeof search);
                    snprintf(search.query,sizeof search.query,"%s",term);
                    reasoning.retrieval_attempted=1;
                    rc=host->invoke_service(DIGIT_CORPUS_SEARCH_SERVICE,
                        &search,sizeof search,&candidate,sizeof candidate,
                        &candidate_used);
                    if(rc==STNLABZ_MODULE_OK&&
                       candidate_used==sizeof candidate&&
                       candidate.count<=DIGIT_CORPUS_RESULTS_MAX){
                        reasoning.retrieval_succeeded=1;
                        if(candidate.count){
                            /* Recency is not verification. Require that
                             * history hits themselves are verified. */
                            unsigned hidx;
                            for(hidx=0;hidx<candidate.count;hidx++){
                                const digit_corpus_hit_t *hit=&candidate.hits[hidx];
                                if((!strcmp(hit->verification_status,"verified")||
                                    !strcmp(hit->verification_status,"operator-verified"))&&
                                   memchr(hit->excerpt,0,sizeof hit->excerpt)&&
                                   hit->excerpt[0]){
                                    found.count=1;found.hits[0]=*hit;
                                    reasoning.corpus=found;break;
                                }
                            }
                            if(found.count)break;
                        }
                    }
                }
            }
        }
        trace_stage(host,"process",DIGIT_CORPUS_SEARCH_SERVICE,
            found.count?"EVIDENCE_RETRIEVED":
            reasoning.retrieval_succeeded?"NO_CORPUS_MATCH":
            "RETRIEVAL_UNAVAILABLE",found.count?0:2,found.count,__LINE__);
        reasoning.context_count=have_history?history.count:0;
        rc=host->invoke_service(DIGIT_REASONING_SERVICE,&reasoning,
            sizeof reasoning,&decision,sizeof decision,&decision_used);
        trace_stage(host,"process","digit.reasoning.evaluate",
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?
              (decision.outcome==DIGIT_REASONING_OUTCOME_EVIDENCE?
                "EVIDENCE_AVAILABLE":decision.outcome==DIGIT_REASONING_OUTCOME_PARTIAL?
                "PARTIAL_EVIDENCE":decision.outcome==DIGIT_REASONING_OUTCOME_CLARIFY?
                "CLARIFICATION_REQUIRED":decision.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN?
                "KNOWLEDGE_UNKNOWN":decision.outcome==DIGIT_REASONING_OUTCOME_FAILURE?
                "RETRIEVAL_FAILED":"EVIDENCE_INSUFFICIENT"):
              rc==STNLABZ_MODULE_ERR_NOT_FOUND?"SERVICE_NOT_FOUND":"SERVICE_ERROR",
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?0:1,
            rc==STNLABZ_MODULE_OK&&decision_used==sizeof decision?
                decision.evidence_count:0,__LINE__);
        if(rc!=STNLABZ_MODULE_OK)return rc;
        if(decision_used!=sizeof decision||
           decision.outcome>DIGIT_REASONING_OUTCOME_PARTIAL||
           decision.reference_count>DIGIT_REASONING_MAX_REFERENCES)
            return STNLABZ_MODULE_ERR_INVALID_STATE;
        /* A skipped View must be diagnosable, not invisible in DSD. */
        if(!participate){
            trace_stage(host,"process","response.view","CHANNEL_OBSERVATION",2,0,__LINE__);
        }else if(!response||!capacity){
            trace_stage(host,"process","response.view","OUTPUT_UNAVAILABLE",2,0,__LINE__);
        }else{
            int rendered=digit_response_render(&decision,&found,&result,msg->message,response,capacity,used);
            const char *status=rendered?
                (decision.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN?
                  "UNKNOWN_STATED":decision.outcome==DIGIT_REASONING_OUTCOME_CLARIFY?
                  "CLARIFICATION_REQUESTED":decision.outcome==DIGIT_REASONING_OUTCOME_FAILURE?
                  "RETRIEVAL_FAILURE_STATED":decision.outcome==DIGIT_REASONING_OUTCOME_PARTIAL?
                  "PARTIAL_KNOWLEDGE_STATED":"RESPONSE_COMPOSED"):
                "EVIDENCE_NOT_RENDERABLE";

            trace_stage(host,"process","response.view",status,
                rendered?0:2,rendered?(unsigned)*used:0,__LINE__);
            if(rendered && used && *used){
                size_t before=*used;
                const char *quote=NULL;
                char generated[512]={0};size_t generated_used=0;
                int uncertain=decision.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN||
                    decision.outcome==DIGIT_REASONING_OUTCOME_CLARIFY;
                if(!uncertain&&found.count&&
                    (strcmp(found.hits[0].verification_status,"verified")==0||
                     strcmp(found.hits[0].verification_status,"operator-verified")==0))
                    quote=found.hits[0].excerpt;
                if(gru_generate_response(host,msg->message,
                     uncertain?NULL:response,quote,
                     generated,sizeof generated,&generated_used)&&
                     generated_used<capacity){
                    memcpy(response,generated,generated_used+1);
                    *used=generated_used;
                }
                trace_stage(host,"process",DIGIT_GRU_GENERATE_SERVICE,
                    *used!=before?"CANDIDATE_ACCEPTED":"FALLBACK_RETAINED",
                    0,(unsigned)*used,__LINE__);
            }else if(participate){
                /* Reasoning may abstain without yielding a View message.
                 * GRU is the language generation path for this case. */
                int generated=gru_generate_response(host,msg->message,NULL,NULL,
                    response,capacity,used);
                trace_stage(host,"process",DIGIT_GRU_GENERATE_SERVICE,
                    generated?"CONVERSATIONAL_RESPONSE":"NO_GENERATED_RESPONSE",
                    generated?0:2,generated?(unsigned)*used:0,__LINE__);
            }
        }
    }else{
        /* Interpretation did not classify an information inquiry. This
         * is still a possible conversational instruction or social turn. */
        int generated=participate&&response&&capacity&&
            gru_generate_response(host,msg->message,NULL,NULL,
                response,capacity,used);
        trace_stage(host,"process",DIGIT_GRU_GENERATE_SERVICE,
            generated?"CONVERSATIONAL_RESPONSE":"NO_GENERATED_RESPONSE",
            generated?0:2,generated?(unsigned)*used:0,__LINE__);
    }

    /* Observation persistence and context creation are atomic in SQLite3.
     * Conversation reads that context; it never duplicates observation writes.
     * A nonempty IRC account remains required for this channel route. */
    if(have_history){
        if (response && capacity && (!used || !*used)) {
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
    /* Generated replies cannot self-authorize training.
     * Reviewed learning remains the responsibility of gru_learning. */
    /* Addressed channel messages may return the View result. Passive
     * channel messages remain observations, not obligatory replies. */
    trace_stage(host,"process","participation",
      used&&*used?"RESPONSE_RETURNED":"RESPONSE_ABSTAIN",0,
      used?(unsigned)*used:0,__LINE__);
    return STNLABZ_MODULE_OK;
}

static stnlabz_module_result_t process(const stnlabz_module_host_t *host,
    const void *request,size_t bytes,void *response,size_t capacity,size_t *used)
{
    return process_route(host,request,bytes,response,capacity,used,0);
}

static stnlabz_module_result_t listen(const void *request,size_t bytes,
    void *response,size_t capacity,size_t *used,void *context)
{
    (void)context;
    return process_route(host_api,request,bytes,response,capacity,used,1);
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
    if(!strcmp(service,DIGIT_GRU_GENERATE_SERVICE)){
        const digit_gru_generate_request_t *in=request;
        digit_gru_generate_result_t out={0};
        if(used)*used=0;
        if(mock_mode!=11)return STNLABZ_MODULE_ERR_NOT_FOUND;
        if(!in||bytes!=sizeof *in||!response||capacity<sizeof out||
           !used||!memchr(in->prompt,0,sizeof in->prompt))
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        strcpy(out.text,"Language generated by GRU.");
        out.generated_bytes=(uint32_t)strlen(out.text);
        memcpy(response,&out,sizeof out);*used=sizeof out;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_IDENTITY_READ_SERVICE)){
        const digit_identity_request_t *req=request;
        digit_identity_response_t out={{0}};
        if(used)*used=0;
        if(!req||bytes!=sizeof *req||!response||
           capacity<sizeof out||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        switch(req->field){
          case DIGIT_IDENTITY_LINEAGE:
            strcpy(out.text,"foundational autonomous agent");break;
          case DIGIT_IDENTITY_MISSION:
            strcpy(out.text,"Digit supports STN-LABZ through research and engineering.");break;
          case DIGIT_IDENTITY_MOTHER:
            strcpy(out.text,"poe");break;
          default:return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        }
        memcpy(response,&out,sizeof out);*used=sizeof out;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_SOCIAL_INTELLIGENCE_SERVICE)){
        const digit_social_intelligence_request_t *in=request;
        digit_social_intelligence_result_t out={0};
        if(used)*used=0;
        if(!in||bytes!=sizeof *in||!response||
           capacity<sizeof out||!used)
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        if(!strcmp(in->text,":)")||!strcmp(in->text,":xD")){
            out.handled=1;strcpy(out.response,"=^.^=");
        }
        memcpy(response,&out,sizeof out);*used=sizeof out;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_SOCIALS_SERVICE)){
        const digit_socials_request_t *req=request;
        digit_socials_response_t value={{0}};
        if(used)*used=0;
        if(!req||bytes!=sizeof *req||!response||
           capacity<sizeof value||!used||
           strcmp(req->category,"cats"))
            return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        strcpy(value.expression,"=^.^=");
        memcpy(response,&value,sizeof value);*used=sizeof value;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_DSD_CURRENT_SERVICE))return STNLABZ_MODULE_ERR_NOT_FOUND;
    if(!strcmp(service,DIGIT_CORPUS_SEARCH_SERVICE)){
        digit_corpus_result_t empty={0};
        const digit_corpus_query_t *q=request;
        if(used)*used=0;
        if(!request||bytes!=sizeof(digit_corpus_query_t)||!response||
            capacity<sizeof empty||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        if(mock_mode==10&&!strcmp(q->query,"speak")){
            empty.count=1;empty.hits[0].id=7;
            strcpy(empty.hits[0].category,"feline");
            strcpy(empty.hits[0].verification_status,"verified");
            strcpy(empty.hits[0].excerpt,"Speak is a general word that can be applied to many unrelated kinds of communication.");
        }
        if(mock_mode==10&&!strcmp(q->query,"digit")){
            empty.count=1;empty.hits[0].id=8;
            strcpy(empty.hits[0].category,"capability");
            strcpy(empty.hits[0].verification_status,"verified");
            strcpy(empty.hits[0].excerpt,"Digit can understand and use cat speak in contextual social exchanges.");
        }
        if(mock_mode==9&&!strcmp(q->query,"cat")){
            empty.count=1;empty.hits[0].id=99;
            strcpy(empty.hits[0].category,"feline");
            strcpy(empty.hits[0].verification_status,"verified");
            strcpy(empty.hits[0].excerpt,"Cat vocalizations can convey social signals.");
        }
        memcpy(response,&empty,sizeof empty);*used=sizeof empty;
        return STNLABZ_MODULE_OK;
    }
    if(!strcmp(service,DIGIT_REASONING_SERVICE)){
        digit_reasoning_result_t decision={0};
        const digit_reasoning_request_t *req=request;
        if(used)*used=0;
        if(!req||bytes!=sizeof *req||!response||
            capacity<sizeof decision||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
        decision.outcome=mock_mode==7?DIGIT_REASONING_OUTCOME_UNKNOWN:
            (mock_mode==9||mock_mode==10)?DIGIT_REASONING_OUTCOME_EVIDENCE:
            DIGIT_REASONING_OUTCOME_ABSTAIN;
        if(mock_mode==9||mock_mode==10){
            decision.evidence_count=1;decision.reference_count=1;
            decision.corpus_ids[0]=mock_mode==10?8:99;
        }
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
    if(!strcmp(q->text,":)")||!strcmp(q->text,"...")||
       !strcmp(q->text,"?"))r.kind=DIGIT_UTTERANCE_UNRESOLVED;
    if(!strcmp(q->text,"mrrp")||!strcmp(q->text,"does paws lickies")){
       r.kind=DIGIT_UTTERANCE_STATEMENT;
       r.term_count=1;
       strcpy(r.terms[0],!strcmp(q->text,"mrrp")?"mrrp":"paws");
    }else if(!strcmp(q->text,"what is the Chaos Foundation?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=5;
       strcpy(r.terms[0],"what");strcpy(r.terms[1],"is");
       strcpy(r.terms[2],"the");strcpy(r.terms[3],"chaos");
       strcpy(r.terms[4],"foundation");
    }else if(!strcmp(q->text,"can you tell me about the Chaos Foundation")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=8;
       strcpy(r.terms[0],"can");strcpy(r.terms[1],"you");
       strcpy(r.terms[2],"tell");strcpy(r.terms[3],"me");
       strcpy(r.terms[4],"about");strcpy(r.terms[5],"the");
       strcpy(r.terms[6],"chaos");strcpy(r.terms[7],"foundation");
    }else if(!strcmp(q->text,"and then now she is doing stupid shit like this?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=4;
       strcpy(r.terms[0],"and");strcpy(r.terms[1],"then");
       strcpy(r.terms[2],"now");strcpy(r.terms[3],"she");
    }else if(!strcmp(q->text,"no ?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=1;
       strcpy(r.terms[0],"no");
    }else if(!strcmp(q->text,"so her ? works")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=3;
       strcpy(r.terms[0],"so");strcpy(r.terms[1],"her");
       strcpy(r.terms[2],"works");
    }else if(!strcmp(q->text,"does digit understand cat speak?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=5;
       strcpy(r.terms[0],"does");strcpy(r.terms[1],"digit");
       strcpy(r.terms[2],"understand");strcpy(r.terms[3],"cat");
       strcpy(r.terms[4],"speak");
    }else if(!strcmp(q->text,"does she understand cat talk?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=5;
       strcpy(r.terms[0],"does");strcpy(r.terms[1],"she");
       strcpy(r.terms[2],"understand");strcpy(r.terms[3],"cat");
       strcpy(r.terms[4],"talk");r.features=DIGIT_FEATURE_REFERENCE;
       r.unresolved_references=1;
    }else if(!strcmp(q->text,"what are you?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=3;
       strcpy(r.terms[0],"what");strcpy(r.terms[1],"are");strcpy(r.terms[2],"you");
    }else if(!strcmp(q->text,"what is your mission?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=4;
       strcpy(r.terms[0],"what");strcpy(r.terms[1],"is");
       strcpy(r.terms[2],"your");strcpy(r.terms[3],"mission");
    }else if(!strcmp(q->text,"who is your creator?")){
       r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=4;
       strcpy(r.terms[0],"who");strcpy(r.terms[1],"is");
       strcpy(r.terms[2],"your");strcpy(r.terms[3],"creator");
    }

    if (mock_mode == 8 && !strcmp(q->text,":)")){r.kind=DIGIT_UTTERANCE_QUESTION;r.term_count=1;strcpy(r.terms[0],"smile");}
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
    {
        digit_interpretation_result_t identity={0};
        uint32_t field=0;int creator=0;
        identity.term_count=4;
        strcpy(identity.terms[0],"what");strcpy(identity.terms[1],"did");
        strcpy(identity.terms[2],"you");strcpy(identity.terms[3],"today");
        check(q,!core_identity_field(&identity,&field,&creator),1);
        strcpy(identity.terms[1],"are");
        identity.term_count=3;
        identity.terms[3][0]=0;
        check(q,core_identity_field(&identity,&field,&creator)&&
            field==DIGIT_IDENTITY_LINEAGE,0);
    }
    check(q,!gru_generate_response(&mock,"Hello",NULL,NULL,
        response,sizeof response,&used),1);
    {
        message_t sample=msg;size_t count=0;
        strcpy(sample.message,"what is the Chaos Foundation?");
        mock_mode=11;
        check(q,process(&mock,&sample,sizeof sample,response,
              sizeof response,&count)==STNLABZ_MODULE_OK&&
              count>0&&!strcmp(response,"Language generated by GRU."),0);
        mock_mode=0;
        count=0;
        check(q,gru_generate_response(&mock,"A message",NULL,NULL,
              response,sizeof response,&count)==0,1);
    }
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
    {
        message_t addressed=msg;
        size_t emitted=99;
        strcpy(addressed.target,"#stn-labz");
        check(q,process_route(&mock,&addressed,sizeof addressed,response,
           sizeof response,&emitted,1)==STNLABZ_MODULE_OK&&emitted==0,0);
        mock_mode=7;
        emitted=99;
        check(q,process_route(&mock,&addressed,sizeof addressed,response,
           sizeof response,&emitted,1)==STNLABZ_MODULE_OK&&emitted==0,0);
        emitted=99;
        check(q,process_route(&mock,&addressed,sizeof addressed,response,
           sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted==0,0);
        mock_mode=0;
    }
    {
        message_t self=msg;size_t emitted=0;
        strcpy(self.target,"#stn-labz");
        strcpy(self.message,"what are you?");
        check(q,process_route(&mock,&self,sizeof self,response,
             sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
             emitted>0&&strstr(response,"foundational autonomous agent")!=NULL,0);
        strcpy(self.message,"what is your mission?");
        emitted=0;
        check(q,process_route(&mock,&self,sizeof self,response,
             sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
             emitted>0&&strstr(response,"STN-LABZ")!=NULL,0);
        strcpy(self.message,"who is your creator?");
        emitted=0;
        check(q,process_route(&mock,&self,sizeof self,response,
             sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
             emitted>0&&strstr(response,"does not specify my creator")!=NULL,0);
    }
    {
        const char *const engage[]={
            "what is the Chaos Foundation?",
            "can you tell me about the Chaos Foundation"
        };
        const char *const abstain[]={
            "and then now she is doing stupid shit like this?",
            "no ?","so her ? works","Database latency increased"
        };
        message_t channel=msg;
        size_t emitted=0;
        unsigned i;
        strcpy(channel.target,"#stn-labz");
        mock_mode=7;
        for(i=0;i<sizeof engage/sizeof engage[0];i++){
            strcpy(channel.message,engage[i]);emitted=0;
            check(q,process_route(&mock,&channel,sizeof channel,response,
                sizeof response,&emitted,1)==STNLABZ_MODULE_OK&&
                emitted>0&&strstr(response,"don't know")!=NULL,0);
        }
        for(i=0;i<sizeof abstain/sizeof abstain[0];i++){
            strcpy(channel.message,abstain[i]);emitted=99;
            check(q,process_route(&mock,&channel,sizeof channel,response,
                sizeof response,&emitted,1)==STNLABZ_MODULE_OK&&
                emitted==0,1);
        }
        /* Directly addressed social and ordinary statements must not
         * generate a factual uncertainty or clarification response. */
        strcpy(channel.message,":)");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
            emitted>0&&!strcmp(response,"=^.^="),0);
        mock_mode=8;strcpy(channel.message,":)");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
            emitted>0&&!strcmp(response,"=^.^="),0);
        mock_mode=0;strcpy(channel.message,":xD");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
            emitted>0&&!strcmp(response,"=^.^="),0);
        strcpy(channel.message,"...");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted==0,1);
        strcpy(channel.message,"?");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted>0&&
            strstr(response,"I'm here")!=NULL,0);
        mock_mode=9;
        strcpy(channel.message,"does she understand cat talk?");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
            emitted>0&&strstr(response,"clarification")==NULL,0);
        mock_mode=10;
        strcpy(channel.message,"does digit understand cat speak?");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted>0&&
            strstr(response,"Corpus #")==NULL,0);
        mock_mode=0;
        strcpy(channel.message,"good to see you");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted==0,0);
        strcpy(channel.message,"Database latency increased");emitted=99;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&emitted==0,0);
        strcpy(channel.message,"");emitted=0;
        check(q,process_route(&mock,&channel,sizeof channel,response,
            sizeof response,&emitted,0)==STNLABZ_MODULE_OK&&
            emitted>0&&strstr(response,"I'm here")!=NULL,0);
        mock_mode=0;
    }
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
        digit_interpretation_result_t question={0};
        char render[512]={0};size_t length=0;
        data.count=1;data.hits[0].id=42;
        strcpy(data.hits[0].verification_status,"operator-verified");
        strcpy(data.hits[0].excerpt,"PHP is a server-side language. Additional text.");
        decision.outcome=DIGIT_REASONING_OUTCOME_EVIDENCE;
        decision.reference_count=1;decision.corpus_ids[0]=42;
        strcpy(data.hits[0].excerpt,"KItten and Furry speak are their own languages.");
        check(q,digit_response_render(&decision,&data,&question,
            "or is it KItten?",render,sizeof render,&length)&&
            strstr(render,"written as \"KItten\"")!=NULL,0);
        check(q,digit_response_render(&decision,&data,&question,
            "or is it kitten?",render,sizeof render,&length)&&
            strstr(render,"written as \"KItten\"")!=NULL,0);
        strcpy(data.hits[0].excerpt,"PHP is a server-side language. Additional text.");

        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
           &&length>0&&strstr(render,"PHP is a server-side language.")!=NULL&&
           strstr(render,"Corpus #")==NULL,0);
        decision.requires_verification=1;length=9;
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
           &&length==0,1);
        decision.requires_verification=0;
        strcpy(data.hits[0].verification_status,"verified");
        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
           &&length>0,0);
        strcpy(data.hits[0].verification_status,"pending");
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length),1);
        strcpy(data.hits[0].verification_status,"unverified");
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length),1);
        strcpy(data.hits[0].verification_status,"operator-verified");
        strcpy(data.hits[0].excerpt,"Unfinished evidence fragment");
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length),1);
        strcpy(data.hits[0].excerpt,"Short fact.");
        decision.corpus_ids[0]=900;
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length),1);
        decision.corpus_ids[0]=42;
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,5,&length),1);
        decision.outcome=DIGIT_REASONING_OUTCOME_PARTIAL;
        strcpy(data.hits[0].excerpt,"Chaos Foundation maintains engineering documentation.");
        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)&&
          strstr(render,"not a full explanation")!=NULL,0);
        strcpy(data.hits[0].verification_status,"unverified");
        check(q,!digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length),1);
        strcpy(data.hits[0].verification_status,"verified");
        decision.outcome=DIGIT_REASONING_OUTCOME_UNKNOWN;
        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
          &&strstr(render,"don't know")!=NULL,0);
        decision.outcome=DIGIT_REASONING_OUTCOME_CLARIFY;
        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
          &&strstr(render,"clarification")!=NULL,0);
        decision.outcome=DIGIT_REASONING_OUTCOME_FAILURE;
        check(q,digit_response_render(&decision,&data,&question,"What is PHP?",render,sizeof render,&length)
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
    if (!api->register_service(LISTEN, listen, NULL))
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
    .version_major = 1, .version_minor = 2, .version_patch = 7,
    .required_core_api_major = STNLABZ_MODULE_API_MAJOR,
    .required_core_api_minor = STNLABZ_MODULE_API_MINOR,
    .qualify = qualify, .start = start, .stop = stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
