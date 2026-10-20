/* [AI:GPT-6 | 2026-10-20] Evidence-constrained Reasoning ABI 1.5.
 * No scripts, LLM inference, database access, or invented conclusions. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "dsd.h"
#include "module.h"
#include "reasoning.h"

static const stnlabz_module_host_t *runtime_host;

/* Record the actual Reasoning decision at its source, not at Conversation's
 * delivery boundary. DSD is advisory; unavailable tracing cannot alter output. */
static void trace_decision(const digit_reasoning_result_t *decision,
                          unsigned line){
 digit_dsd_record_t record={0};
 uint64_t token=1,trace=0;
 size_t used=0;
 const char *status;
 if(!runtime_host||!runtime_host->invoke_service||!decision)return;
 if(runtime_host->invoke_service(DIGIT_DSD_CURRENT_SERVICE,
    &token,sizeof token,&trace,sizeof trace,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof trace||!trace)return;
 switch(decision->outcome){
  case DIGIT_REASONING_OUTCOME_EVIDENCE:status="EVIDENCE_AVAILABLE";break;
  case DIGIT_REASONING_OUTCOME_UNKNOWN:status="KNOWLEDGE_UNKNOWN";break;
  case DIGIT_REASONING_OUTCOME_CLARIFY:status="CLARIFICATION_REQUIRED";break;
  case DIGIT_REASONING_OUTCOME_FAILURE:status="RETRIEVAL_FAILED";break;
  case DIGIT_REASONING_OUTCOME_CONFLICT:status="EVIDENCE_CONFLICT";break;
  default:status="EVIDENCE_INSUFFICIENT";break;
 }
 record.trace_id=record.event.trace_id=trace;
 record.event.span_id=4;
 record.event.parent_span_id=1;
 record.event.line=line;
 record.event.records=decision->evidence_count;
 record.event.result=decision->outcome==DIGIT_REASONING_OUTCOME_FAILURE?1:
    decision->outcome==DIGIT_REASONING_OUTCOME_EVIDENCE?0:2;
 snprintf(record.event.module,sizeof record.event.module,"reasoning");
 snprintf(record.event.method,sizeof record.event.method,"evaluate");
 snprintf(record.event.file,sizeof record.event.file,
          "modules/reasoning/reasoning.c");
 snprintf(record.event.operation,sizeof record.event.operation,
          "digit.reasoning.evaluate");
 snprintf(record.event.status,sizeof record.event.status,"%s",status);
 (void)runtime_host->invoke_service(DIGIT_DSD_RECORD_SERVICE,
   &record,sizeof record,NULL,0,&used);
}
static int bounded(const char *s,size_t n){
 return s&&memchr(s,0,n)!=NULL;
}
static int validate(const digit_reasoning_request_t *q){
 unsigned i,j;
 if(!q||q->interpretation.kind>DIGIT_UTTERANCE_QUESTION||
    q->interpretation.term_count>DIGIT_INTERPRETATION_TERMS_MAX||
    q->interpretation.unresolved_references>DIGIT_INTERPRETATION_TERMS_MAX||
    q->corpus.count>DIGIT_CORPUS_RESULTS_MAX||
    q->retrieval_attempted>1||q->retrieval_succeeded>1||
    (q->retrieval_succeeded&&!q->retrieval_attempted)||
    (!q->retrieval_succeeded&&q->corpus.count))return 0;
 for(i=0;i<q->interpretation.term_count;i++)
  if(!bounded(q->interpretation.terms[i],DIGIT_INTERPRETATION_TERM_MAX))return 0;
 for(i=0;i<q->corpus.count;i++){
  const digit_corpus_hit_t *h=&q->corpus.hits[i];
  if(h->id<=0||!bounded(h->category,sizeof h->category)||
     !bounded(h->verification_status,sizeof h->verification_status)||
     !bounded(h->excerpt,sizeof h->excerpt))return 0;
  for(j=0;j<i;j++)
   if(q->corpus.hits[j].id==h->id)return 0;
 }
 return 1;
}
/* Eligibility is not factual verification. Reject isolated terms and evidence
 * unrelated to the observed subject; never infer a definition from a match. */
static int subject_occurs(const char *source,const char *term){
 size_t i,n=strlen(term);
 if(!n)return 0;
 for(i=0;source[i];i++){
  size_t j;
  if(i&&isalnum((unsigned char)source[i-1]))continue;
  for(j=0;j<n&&source[i+j]&&
      tolower((unsigned char)source[i+j])==
      tolower((unsigned char)term[j]);j++);
  if(j==n&&!isalnum((unsigned char)source[i+j]))return 1;
 }
 return 0;
}
/* Explicit allowlist of stored Corpus verification labels. These remain
 * distinct provenance values; neither is rewritten or promoted. */
static int evidence_eligible(const char *status){
 return !strcmp(status,"operator-verified")||!strcmp(status,"verified");
}
static int informative(const char *source,const char *topic){
 size_t i=0,words=0,letters=0;
 int inword=0,detail=0;
 if(!source||!memchr(source,0,DIGIT_CORPUS_EXCERPT_MAX))return 0;
 for(;source[i];i++){
  unsigned char c=(unsigned char)source[i];
  if(isalpha(c)){letters++;if(!inword){words++;inword=1;}}
  else if(isspace(c)||ispunct(c)){inword=0;if(c=='.'||c==';'||c==':')detail=1;}
  else return 0;
 }
 /* Minimum explanatory substance is a necessary, not sufficient test.
  * A bare title, keyword, or a short label cannot qualify. */
 return letters>=24&&words>=6&&detail&&subject_occurs(source,topic);
}
static void evaluate(const digit_reasoning_request_t *q,
 digit_reasoning_result_t *out){
 unsigned i,j;
 const char *topic=NULL;
 memset(out,0,sizeof *out);
 out->outcome=DIGIT_REASONING_OUTCOME_ABSTAIN;
 out->uncertainty=1;
 out->requires_verification=1;
 if(q->interpretation.kind==DIGIT_UTTERANCE_UNRESOLVED||
    (q->interpretation.features&DIGIT_FEATURE_TRUNCATED)||
    q->interpretation.unresolved_references){
  out->outcome=DIGIT_REASONING_OUTCOME_CLARIFY;
  out->requires_clarification=1;
  return;
 }
 if(!q->retrieval_attempted||!q->retrieval_succeeded){
  out->outcome=DIGIT_REASONING_OUTCOME_FAILURE;
  return;
 }
 if(!q->corpus.count){
  out->outcome=DIGIT_REASONING_OUTCOME_UNKNOWN;
  return;
 }
 /* An unqualified one-word utterance is not an instruction to recite the
  * first matching entry. Preserve lexical ambiguity as a clarification. */
 if(q->interpretation.term_count==1&&
    !(q->interpretation.features&DIGIT_FEATURE_EXPLANATION_REQUEST)){
  out->outcome=DIGIT_REASONING_OUTCOME_CLARIFY;
  out->requires_clarification=1;
  return;
 }
 /* Interpretation currently retains ordered lexical terms. Recognize the
  * grammatical command as intent, never as the search subject. */
 for(j=q->interpretation.term_count;j>0;j--){
  const char *term=q->interpretation.terms[j-1];
  if(strcmp(term,"explain")&&strcmp(term,"describe")&&
     strcmp(term,"what")&&strcmp(term,"is")&&strcmp(term,"the")&&
     strcmp(term,"a")&&strcmp(term,"an")){
   topic=term;break;
  }
 }
 if(!topic){out->outcome=DIGIT_REASONING_OUTCOME_CLARIFY;
  out->requires_clarification=1;return;}
 for(i=0;i<q->corpus.count;i++){
  const digit_corpus_hit_t *h=&q->corpus.hits[i];
  if(evidence_eligible(h->verification_status)&&
     informative(h->excerpt,topic)&&
     out->reference_count<DIGIT_REASONING_MAX_REFERENCES)
   out->corpus_ids[out->reference_count++]=h->id;
 }
 if(!out->reference_count)out->outcome=DIGIT_REASONING_OUTCOME_UNKNOWN;
 out->evidence_count=out->reference_count;
 if(out->evidence_count){
  out->outcome=DIGIT_REASONING_OUTCOME_EVIDENCE;
  out->requires_verification=0;
  /* Lexical relevance cannot establish truth or semantic entailment. */
  out->uncertainty=1;
 }
}
static stnlabz_module_result_t service(
 const void *request,size_t bytes,void *response,size_t capacity,
 size_t *used,void *ctx){
 digit_reasoning_result_t r={0};
 (void)ctx;
 if(used)*used=0;
 if(!request||bytes!=sizeof(digit_reasoning_request_t)||
    !response||capacity<sizeof r||!used||
    !validate((const digit_reasoning_request_t *)request))
   return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 evaluate((const digit_reasoning_request_t *)request,&r);
 trace_decision(&r,__LINE__);
 memcpy(response,&r,sizeof r);
 *used=sizeof r;
 return STNLABZ_MODULE_OK;
}
static void check(stnlabz_module_qualification_result_t *q,int ok,int neg){
 q->tests_executed++;
 if(ok)q->tests_passed++;else q->tests_failed++;
 if(neg){q->negative_test_executed=1;if(ok)q->negative_test_passed=1;}
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_reasoning_request_t request={0},mutated;
 digit_reasoning_result_t response={0};size_t used;
 unsigned i;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 request.interpretation.kind=DIGIT_UTTERANCE_QUESTION;
 request.interpretation.features=DIGIT_FEATURE_EXPLANATION_REQUEST;
 request.interpretation.term_count=1;
 strcpy(request.interpretation.terms[0],"php");
 request.retrieval_attempted=request.retrieval_succeeded=1;
 request.corpus.count=1;request.corpus.hits[0].id=123;
 strcpy(request.corpus.hits[0].category,"documentation");
 strcpy(request.corpus.hits[0].verification_status,"operator-verified");
 strcpy(request.corpus.hits[0].excerpt,"PHP is a language used to create dynamically generated web pages.");
 used=0;
 check(q,service(&request,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK,0);
 check(q,used==sizeof response,0);
 check(q,response.outcome==DIGIT_REASONING_OUTCOME_EVIDENCE,0);
 check(q,response.corpus_ids[0]==123,0);
 check(q,response.evidence_count==1,0);
 check(q,response.uncertainty==1,0);
 check(q,response.requires_verification==0,0);
 check(q,response.requires_clarification==0,0);
 mutated=request;strcpy(mutated.corpus.hits[0].excerpt,"PHP");
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN,1);
 mutated=request;strcpy(mutated.corpus.hits[0].excerpt,"Python is a language used to create dynamically generated web pages.");
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN,1);
 mutated=request;mutated.interpretation.features=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_CLARIFY,1);
 mutated=request;mutated.corpus.count=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN,0);
 mutated=request;strcpy(mutated.corpus.hits[0].verification_status,"verified");
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&
   response.outcome==DIGIT_REASONING_OUTCOME_EVIDENCE&&response.corpus_ids[0]==123,0);
 mutated=request;strcpy(mutated.corpus.hits[0].verification_status,"pending");
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&
   response.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN,1);
 mutated=request;strcpy(mutated.corpus.hits[0].verification_status,"unverified");
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.evidence_count==0&&response.outcome==DIGIT_REASONING_OUTCOME_UNKNOWN,0);
 mutated=request;mutated.interpretation.unresolved_references=1;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.requires_clarification,0);
 mutated=request;mutated.interpretation.features=DIGIT_FEATURE_TRUNCATED;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_CLARIFY,0);
 mutated=request;mutated.interpretation.kind=DIGIT_UTTERANCE_UNRESOLVED;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_CLARIFY,0);
 mutated=request;mutated.retrieval_attempted=0;mutated.retrieval_succeeded=0;mutated.corpus.count=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_OK&&response.outcome==DIGIT_REASONING_OUTCOME_FAILURE,0);
 for(i=0;i<3;i++){
  mutated=request;
  if(i==0)mutated.corpus.count=DIGIT_CORPUS_RESULTS_MAX+1;
  if(i==1)mutated.interpretation.term_count=DIGIT_INTERPRETATION_TERMS_MAX+1;
  if(i==2)mutated.interpretation.unresolved_references=DIGIT_INTERPRETATION_TERMS_MAX+1;
  check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 }
 mutated=request;mutated.retrieval_attempted=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 mutated=request;mutated.retrieval_succeeded=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 mutated=request;mutated.corpus.hits[0].id=0;
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 mutated=request;memset(mutated.corpus.hits[0].excerpt,'x',sizeof mutated.corpus.hits[0].excerpt);
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 mutated=request;memset(mutated.interpretation.terms[0],'x',sizeof mutated.interpretation.terms[0]);
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 used=5;
 check(q,service(NULL,sizeof request,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT&&used==0,1);
 check(q,service(&request,sizeof request-1,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,service(&request,sizeof request,NULL,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,service(&request,sizeof request,&response,sizeof response-1,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,service(&request,sizeof request,&response,sizeof response,NULL,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 mutated=request;mutated.corpus.count=2;mutated.corpus.hits[1]=mutated.corpus.hits[0];
 check(q,service(&mutated,sizeof mutated,&response,sizeof response,&used,NULL)==STNLABZ_MODULE_ERR_INVALID_ARGUMENT,1);
 check(q,sizeof(response.corpus_ids)/sizeof(response.corpus_ids[0])==DIGIT_REASONING_MAX_REFERENCES,0);
 return q->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&!q->tests_failed&&
   q->negative_test_executed&&q->negative_test_passed?
   STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *api){
 if(runtime_host||!api||!api->register_service||!api->unregister_service||
    !api->invoke_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!api->register_service(DIGIT_REASONING_SERVICE,service,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 runtime_host=api;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!runtime_host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!runtime_host->unregister_service(DIGIT_REASONING_SERVICE,NULL))
  return STNLABZ_MODULE_ERR_STOP_FAILED;
 runtime_host=NULL;return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 .id="reasoning",.name="Evidence Constrained Reasoning",
 .version_major=0,.version_minor=1,.version_patch=5,
 .required_core_api_major=STNLABZ_MODULE_API_MAJOR,
 .required_core_api_minor=STNLABZ_MODULE_API_MINOR,
 .qualify=qualify,.start=start,.stop=stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
