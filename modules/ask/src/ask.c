#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "stnlabz_abi/module.h"
#include "ask_context.h"
#include "corpus_lookup.h"
#include "interpretation.h"
#include "research_contract.h"
#include "observation.h"
#include "reasoning.h"
#include "socials.h"
static const stnlabz_module_host_t *host;
/* In IRC, a non-channel target is a private conversation addressed to Digit. */
static int irc_private_context(const digit_ask_context_t *c){
 return c&&!strcmp(c->transport,"irc")&&c->channel[0]&&c->channel[0]!='#';
}
static int classify_context_message(const digit_ask_context_t *c,const char *message,
 digit_interpret_request_t *request,digit_interpret_response_t *interpreted){
 size_t used=0;stnlabz_module_result_t rc;
 request->version=DIGIT_INTERPRETATION_VERSION;
 snprintf(request->text,sizeof request->text,"%s",message);
 if(!host||!host->invoke_service)return 0;
 rc=host->invoke_service("interpretation.classify",request,sizeof *request,
                          interpreted,sizeof *interpreted,&used);
 if(rc!=STNLABZ_MODULE_OK||used!=sizeof *interpreted||
    interpreted->version!=DIGIT_INTERPRETATION_VERSION)return 0;
 if(irc_private_context(c)&&!interpreted->addressed){
  if(snprintf(request->text,sizeof request->text,"Digit %s",message)>=(int)sizeof request->text)return 0;
  used=0;
  rc=host->invoke_service("interpretation.classify",request,sizeof *request,
                           interpreted,sizeof *interpreted,&used);
  if(rc!=STNLABZ_MODULE_OK||used!=sizeof *interpreted||
     interpreted->version!=DIGIT_INTERPRETATION_VERSION)return 0;
 }
 return 1;
}
/* Social expressions are optional; Socials owns the symbols and selection. */
static void append_social(const char *key, char *out,size_t capacity,size_t *used) {
 digit_socials_request_t q={0};
 digit_socials_response_t r={0};
 size_t n=0,len,emoji_len;
 if(!host||!host->invoke_service||!key||!out||!used)return;
 q.version=DIGIT_SOCIALS_VERSION;q.category=DIGIT_SOCIALS_ANY;
 if(snprintf(q.context,sizeof q.context,"%s",key)>=(int)sizeof q.context)return;
 if(host->invoke_service(DIGIT_SOCIALS_SERVICE,&q,sizeof q,&r,sizeof r,&n)!=STNLABZ_MODULE_OK||
    n!=sizeof r||r.version!=DIGIT_SOCIALS_VERSION||
    !memchr(r.emoji,0,sizeof r.emoji)||!r.emoji[0])return;
 len=strlen(out);emoji_len=strlen(r.emoji);
 if(len+1+emoji_len+1>capacity)return;
 out[len]=' ';memcpy(out+len+1,r.emoji,emoji_len+1);
 *used=len+1+emoji_len+1;
}

/* Corpus evidence is selected by Interpretation's normalized subject. */
static void greeting_evidence(const char *subject,char *text,size_t capacity){
 digit_corpus_lookup_request_t q={0};
 digit_corpus_lookup_response_t r={0};
 size_t n=0;
 if(!host||!host->invoke_service||!subject||!subject[0]||
    strlen(subject)>=sizeof q.term)return;
 snprintf(q.term,sizeof q.term,"%s",subject);
 if(host->invoke_service("corpus.lookup",&q,sizeof q,&r,sizeof r,&n)==STNLABZ_MODULE_OK&&
    n==sizeof r&&r.found&&memchr(r.text,0,sizeof r.text)&&
    memchr(r.id,0,sizeof r.id)&&memchr(r.source,0,sizeof r.source))
  snprintf(text,capacity,"%s",r.text);
}
static stnlabz_module_result_t research_answer(const char *subject,
 void *response,size_t capacity,size_t *used){
 digit_research_request_t query={0};
 digit_research_result_t *found;
 stnlabz_module_result_t rc;
 const char *url=NULL,*mission="Operator-requested public research";
 size_t n=0,i;int written;
 char excerpt[320];size_t count=0;
 if(!subject||!response||!used||capacity<2)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 while(*subject==' ')subject++;
 if(!strncasecmp(subject,"https://",8))url=subject;
 else if(!strcasecmp(subject,"PHP")||!strncasecmp(subject,"PHP ",4))
  url="https://www.php.net/releases/";
 if(!url){
  const char *message="Research source discovery is not available for this topic yet. Supply a public HTTPS URL, or ask me to research PHP.";
  if(strlen(message)+1>capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  strcpy(response,message);*used=strlen(message)+1;
  return STNLABZ_MODULE_OK;
 }
 if(strlen(url)>=sizeof query.url)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 found=calloc(1,sizeof *found);
 if(!found)return STNLABZ_MODULE_ERR_START_FAILED;
 query.version=DIGIT_RESEARCH_CONTRACT_VERSION;
 snprintf(query.url,sizeof query.url,"%s",url);
 snprintf(query.mission,sizeof query.mission,"%s",mission);
 rc=!host||!host->invoke_service?STNLABZ_MODULE_ERR_NOT_FOUND:
  host->invoke_service(DIGIT_RESEARCH_SERVICE,&query,sizeof query,
                       found,sizeof *found,&n);
 if(rc!=STNLABZ_MODULE_OK||n!=sizeof *found||
    found->version!=DIGIT_RESEARCH_CONTRACT_VERSION||
    !memchr(found->excerpt,0,sizeof found->excerpt)||
    !memchr(found->url,0,sizeof found->url)){
  const char *message="Research source retrieval unavailable; no findings verified.";
  if(strlen(message)+1>capacity){free(found);return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;}
  strcpy(response,message);*used=strlen(message)+1;
  free(found);return STNLABZ_MODULE_OK;
 }
 for(i=0;found->excerpt[i]&&count<sizeof excerpt-1;i++){
  unsigned char ch=(unsigned char)found->excerpt[i];
  if(ch=='\r'||ch=='\n'||ch=='\t')ch=' ';
  if(ch<32||ch>126)continue;
  excerpt[count++]=(char)ch;
 }
 excerpt[count]=0;
 written=snprintf(response,capacity,
  "Retrieved source: %s\nUTC: %s\nHTTP: %d\nUnverified source excerpt: %.260s\n"
  "This is retrieved text, not a vulnerability assessment.",
  found->url,found->retrieved_at,found->http_status,excerpt);
 free(found);
 if(written<0||(size_t)written>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 *used=(size_t)written+1;
 return STNLABZ_MODULE_OK;
}
/* Resolve ambient topics using Corpus, not a predefined subject vocabulary.
 * Bounded multiword spans preserve original order and require a Corpus match. */
static int contextual_corpus_lookup(const char *message,const char *subject,
 digit_corpus_lookup_response_t *result){
 digit_corpus_lookup_request_t q={0};
 const char *begin[80],*end[80],*p=message;
 size_t count=0,n=0,words,start;
 /* A selected subject is a candidate, not a terminal search result.
    On a genuine miss, search evidence-bearing phrases from the full message. */
 if(subject&&subject[0]&&strlen(subject)<sizeof q.term){
  snprintf(q.term,sizeof q.term,"%s",subject);
  if(host->invoke_service("corpus.lookup",&q,sizeof q,result,
     sizeof *result,&n)==STNLABZ_MODULE_OK&&n==sizeof *result&&result->found)
   return 1;
  memset(result,0,sizeof *result);n=0;
 }
 if(strlen(message)<sizeof q.term){
  snprintf(q.term,sizeof q.term,"%s",message);
  if(host->invoke_service("corpus.lookup",&q,sizeof q,result,sizeof *result,&n)
     ==STNLABZ_MODULE_OK&&n==sizeof *result&&result->found)return 1;
 }
 while(*p&&count<80){
  while(*p&&!isalnum((unsigned char)*p))p++;
  if(!*p)break;
  begin[count]=p;
  while(*p&&(isalnum((unsigned char)*p)||*p=='-'||*p=='_'))p++;
  end[count]=p;count++;
 }
 for(words=4;words>=2;words--){
  for(start=0;start+words<=count;start++){
   size_t len=(size_t)(end[start+words-1]-begin[start]);
   if(len<7||len>=sizeof q.term)continue;
   memcpy(q.term,begin[start],len);q.term[len]=0;n=0;
   if(host->invoke_service("corpus.lookup",&q,sizeof q,result,sizeof *result,&n)
      ==STNLABZ_MODULE_OK&&n==sizeof *result&&result->found)return 1;
  }
 }
 return 0;
}
/* Keep ambient speech short, factual and complete. Prefer an earlier
 * complete clause; if unavailable, use a bounded complete first sentence.
 * Never manufacture a summary or suppress supported evidence for word count. */
static int ambient_fact(const char *src,char *out,size_t cap,size_t *used){
 const size_t limit=320;
 const char *p,*end=NULL,*cut=NULL;
 size_t n,i;
 static const char *const clauses[]={" intended to "," which "," that "," because ",NULL};
 if(!src||!*src||!out||!used||cap<2)return 0;
 for(p=src;*p&&(size_t)(p-src)<limit;p++){
  if((*p=='.'||*p=='!'||*p=='?')&&(!p[1]||isspace((unsigned char)p[1]))){
   end=p+1;break;
  }
 }
 /* [AI:GPT-6 | 2026-10-17 02:49:54 UTC]
  * Preserve short, complete evidence statements without terminal punctuation.
  * Long unpunctuated records still produce silence instead of a dump. */
 if(!end){
  n=strlen(src);
  while(n&&isspace((unsigned char)src[n-1]))n--;
  if(!n||n>120||n+2>cap)return 0;
  memcpy(out,src,n);
  if(out[n-1]!='.'&&out[n-1]!='!'&&out[n-1]!='?')out[n++]='.';
  out[n]=0;*used=n+1;return 1;
 }
 /* [End AI:GPT-6] */
 /* Only shorten at the start of a subordinate clause, never mid-word. */
 if((size_t)(end-src)>100)for(i=0;clauses[i];i++){
  const char *candidate=strstr(src,clauses[i]);
  if(candidate&&candidate>src+28&&candidate<end&&
     (!cut||candidate<cut))cut=candidate;
 }
 if(cut)end=cut;
 n=(size_t)(end-src);
 while(n&&isspace((unsigned char)src[n-1]))n--;
 if(!n||n+2>cap)return 0;
 memcpy(out,src,n);
 if(out[n-1]!='.'&&out[n-1]!='!'&&out[n-1]!='?')out[n++]='.';
 out[n]=0;*used=n+1;return 1;
}
/* Reject ambient echo of an existing participant's statement. Ignore only
 * presentation punctuation and case: this is duplicate detection, not a
 * claim that matching words establish relevance or a reason to speak. */
static int same_statement(const char *a,const char *b){
 size_t i=0,j=0,ai=0,bj=0;
 if(!a||!b)return 0;
 while(a[i]&&isspace((unsigned char)a[i]))i++;
 while(b[j]&&isspace((unsigned char)b[j]))j++;
 ai=strlen(a);bj=strlen(b);
 while(ai>i&&(isspace((unsigned char)a[ai-1])||a[ai-1]=='.'||a[ai-1]=='!'||a[ai-1]=='?'))ai--;
 while(bj>j&&(isspace((unsigned char)b[bj-1])||b[bj-1]=='.'||b[bj-1]=='!'||b[bj-1]=='?'))bj--;
 if(ai-i!=bj-j)return 0;
 while(i<ai&&j<bj){
  if(tolower((unsigned char)a[i])!=tolower((unsigned char)b[j]))return 0;
  i++;j++;
 }
 return 1;
}
/* Evidence-bound fallback for unsupported chat, never an inferred command.
   Mention admission alone is not a reason to speak. */
static stnlabz_module_result_t contextual_answer(const digit_ask_context_t *ctx,
 const char *message,const char *subject,unsigned addressed,void *output,size_t capacity,size_t *used){
 digit_observation_recent_request_t oq={0};
 digit_observation_recent_response_t *history=NULL;
 digit_reasoning_request_t *rq=NULL;
 digit_reasoning_response_t *rr=NULL;
 digit_corpus_lookup_response_t *cr=NULL;
 size_t n=0,i;
 char *out=output;
 int result=0,corpus_ok=0;
 if(!ctx||!ctx->transport[0]||!ctx->channel[0]||
    !ctx->sender[0]||!host||!host->invoke_service){
  out[0]=0;*used=1;return STNLABZ_MODULE_OK;
 }
 if((strcmp(ctx->transport,"irc")&&strcmp(ctx->transport,"slack"))||
    (ctx->transport[0]=='i'&&ctx->channel[0]!='#'&&!irc_private_context(ctx))){
  out[0]=0;*used=1;return STNLABZ_MODULE_OK;
 }
 history=calloc(1,sizeof *history);
 rq=calloc(1,sizeof *rq);
 rr=calloc(1,sizeof *rr);
 cr=calloc(1,sizeof *cr);
 if(!history||!rq||!rr||!cr)goto done;
 oq.version=DIGIT_OBSERVATION_READ_VERSION;
 snprintf(oq.transport,sizeof oq.transport,"%s",ctx->transport);
 snprintf(oq.channel,sizeof oq.channel,"%s",ctx->channel);
 oq.limit=DIGIT_OBSERVATION_RECENT_MAX;
 rq->version=DIGIT_REASONING_VERSION;rq->addressed=addressed;
 snprintf(rq->message,sizeof rq->message,"%s",message);
 snprintf(rq->transport,sizeof rq->transport,"%s",ctx->transport);
 snprintf(rq->channel,sizeof rq->channel,"%s",ctx->channel);
 snprintf(rq->sender,sizeof rq->sender,"%s",ctx->sender);
 if(host->invoke_service(DIGIT_OBSERVATION_READ_SERVICE,&oq,sizeof oq,
                          history,sizeof *history,&n)==STNLABZ_MODULE_OK&&
    n==sizeof *history&&history->version==DIGIT_OBSERVATION_READ_VERSION&&
    history->count<=DIGIT_OBSERVATION_RECENT_MAX){
  rq->observation_state=history->count?DIGIT_REASONING_PRESENT:DIGIT_REASONING_EMPTY;
  for(i=0;i<history->count;i++){
   /* The current message is not independent historical corroboration. */
   if(!strcmp(history->items[i].sender,ctx->sender)&&
      !strcmp(history->items[i].text,message))continue;
   if(rq->observation_count>=DIGIT_REASONING_ITEMS)break;
   snprintf(rq->observations[rq->observation_count].sender,
       sizeof rq->observations[0].sender,"%s",history->items[i].sender);
   snprintf(rq->observations[rq->observation_count].event_id,
       sizeof rq->observations[0].event_id,"%s",history->items[i].event_id);
   snprintf(rq->observations[rq->observation_count].text,
       sizeof rq->observations[0].text,"%s",history->items[i].text);
   rq->observation_count++;
  }
 }else rq->observation_state=DIGIT_REASONING_UNAVAILABLE;
 /* Corpus finds actual subject phrases in the utterance. */
 if(contextual_corpus_lookup(message,subject,cr)){
  corpus_ok=1;
  rq->corpus_state=cr->found?DIGIT_REASONING_PRESENT:DIGIT_REASONING_EMPTY;
  if(cr->found&&memchr(cr->id,0,sizeof cr->id)&&
     memchr(cr->source,0,sizeof cr->source)&&memchr(cr->text,0,sizeof cr->text)){
   snprintf(rq->corpus_id,sizeof rq->corpus_id,"%s",cr->id);
   snprintf(rq->corpus_source,sizeof rq->corpus_source,"%s",cr->source);
   snprintf(rq->corpus_text,sizeof rq->corpus_text,"%s",cr->text);
  }else if(cr->found)rq->corpus_state=DIGIT_REASONING_UNAVAILABLE;
 }else rq->corpus_state=DIGIT_REASONING_UNAVAILABLE;
 if(host->invoke_service(DIGIT_REASONING_SERVICE,rq,sizeof *rq,
         rr,sizeof *rr,&n)!=STNLABZ_MODULE_OK||
    n!=sizeof *rr||rr->version!=DIGIT_REASONING_VERSION)goto done;
 /* Reasoning determines response eligibility for direct invocation.
    Preserve existing query handling for non-addressed requests. */
 if(rr->response_eligible||(rr->knowledge==DIGIT_REASONING_CORPUS_SUPPORTED&&
     rr->relevance==DIGIT_REASONING_CONTEXTUAL&&rr->matched_observations>=1)){
  if(rr->knowledge==DIGIT_REASONING_CORPUS_SUPPORTED&&corpus_ok){
   if(!addressed){
    if(!ambient_fact(cr->text,out,capacity,used)){
     out[0]=0;*used=1;
    }else{
     /* Corpus is knowledge, not permission to echo participants.
      * Do not reply with a statement already present in this channel. */
     int repeated=same_statement(out,message);
     for(i=0;!repeated&&i<rq->observation_count;i++)
      if(same_statement(out,rq->observations[i].text))repeated=1;
     if(repeated){out[0]=0;*used=1;}
    }
    result=1;
   }else{
    int w=snprintf(out,capacity,"%s [Corpus: %s; source: %s]",
                   cr->text,cr->id,cr->source);
    if(w>0&&(size_t)w<capacity){*used=(size_t)w+1;result=1;}
   }
  }else {
   const char *explain=rr->response_eligible?
     "I received your message. Could you clarify what you\'d like me to address?":
     rr->knowledge==DIGIT_REASONING_EVIDENCE_UNAVAILABLE?
     "I do not know; Corpus evidence is unavailable.":
     "I do not know; I found no supporting Corpus record for that question.";
   if(strlen(explain)<capacity){strcpy(out,explain);*used=strlen(explain)+1;result=1;}
  }
 }else {
  /* Contextual word overlap does NOT establish conversational intent.
     Without such evidence we must not interrupt. */
  out[0]=0;*used=1;result=1;
 }
done:
 free(history);free(rq);free(rr);free(cr);
 return result?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_NOT_FOUND;
}

static stnlabz_module_result_t answer(const void *request,size_t length,
 void *response,size_t capacity,size_t *used,void *context){
 const char *s=request;char *out=response;const char *reply;
 const digit_ask_context_t *c=NULL;
 (void)context;
 if(length==sizeof(digit_ask_context_t)){
  c=request;
  if(!c||c->magic!=DIGIT_ASK_CONTEXT_MAGIC||
     !memchr(c->question,0,sizeof c->question))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  s=c->question;length=strlen(s)+1;
 }
 if(!s||!length||length>513||s[length-1]!=0||!response||!used||capacity<2||
    memchr(s,0,length-1))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!s[0])return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 /* Channel traffic is admitted for evaluation, never automatically for speech.
    Interpret without altering the original utterance and gate unaddressed
    contributions on independently corroborated Corpus/Observation evidence. */
 if(c&&c->transport[0]&&c->channel[0]){
  digit_interpret_request_t iq={0};
  digit_interpret_response_t ir={0};
  if(!classify_context_message(c,s,&iq,&ir)){
   out[0]=0;*used=1;return STNLABZ_MODULE_OK;
  }
  if(!ir.addressed&&!irc_private_context(c)){
   return contextual_answer(c,s,ir.subject,0,response,capacity,used);
  }
 }
 if(!strncasecmp(s,"research ",9)){
  return research_answer(s+9,response,capacity,used);
 }
 else if(!strncasecmp(s,"research: ",10)){
  return research_answer(s+10,response,capacity,used);
 }
 else if(!strcasecmp(s,"version")||!strcasecmp(s,"version?")||
         !strcasecmp(s,"what version are you?")||
         !strcasecmp(s,"what is your version?")||
         !strcasecmp(s,"digit version")){
  const char empty=0;char version[32]={0};size_t n=0;int formatted;
  if(!host||!host->invoke_service)return STNLABZ_MODULE_ERR_NOT_FOUND;
  if(host->invoke_service("core.version.read",&empty,sizeof empty,
      version,sizeof version,&n)!=STNLABZ_MODULE_OK||
      n<2||n>sizeof version||!memchr(version,0,n))
   return STNLABZ_MODULE_ERR_NOT_FOUND;
  formatted=snprintf(out,capacity,"Digit %s",version);
  if(formatted<0||(size_t)formatted>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
  *used=(size_t)formatted+1;return STNLABZ_MODULE_OK;
 }
 else if(!strcasecmp(s,"who are you?")||!strcasecmp(s,"who are you")||
         !strcasecmp(s,"what is your name?")||!strcasecmp(s,"what are you?")){
  const char empty=0;size_t n=0;
  if(!host||!host->invoke_service)return STNLABZ_MODULE_ERR_NOT_FOUND;
  return host->invoke_service("core.identity.read",&empty,sizeof empty,
                              response,capacity,&n)==STNLABZ_MODULE_OK&&
         n>0&&n<=capacity&&memchr(response,0,n)?
         (*used=n,STNLABZ_MODULE_OK):STNLABZ_MODULE_ERR_NOT_FOUND;
 }else if(!strcasecmp(s,"diagnose my last request")||
          !strcasecmp(s,"diagnose last request")||
          !strcasecmp(s,"diagnose")){
  digit_scoped_diagnostic_t observation={0};size_t n=0;
  if(!c||!c->has_previous)
   reply="DSD: No prior request in this conversation to diagnose.";
  else {
   if(!host||!host->invoke_service)return STNLABZ_MODULE_ERR_NOT_FOUND;
   memcpy(observation.request_id,c->previous_request,sizeof observation.request_id);
   memcpy(observation.route,c->previous_route,sizeof observation.route);
   memcpy(observation.service,c->previous_service,sizeof observation.service);
   observation.outcome=c->previous_outcome;
   observation.service_result=c->previous_service_result;
   if(host->invoke_service("dsd.scoped",&observation,sizeof observation,
                            response,capacity,&n)!=STNLABZ_MODULE_OK||
      !n||n>capacity||!memchr(response,0,n))
    return STNLABZ_MODULE_ERR_NOT_FOUND;
   *used=n;return STNLABZ_MODULE_OK;
  }
 }else if(!irc_private_context(c)&&!strncasecmp(s,"what is ",8)){
  const char *term=s+8;size_t n=strlen(term),used_lookup=0;
  digit_corpus_lookup_request_t query={0};
  digit_corpus_lookup_response_t found={0};
  int written;
  while(n&&(term[n-1]=='?'||term[n-1]=='.'||term[n-1]==' '))n--;
  if(!n||n>=sizeof query.term)
   reply="Corpus: invalid knowledge query.";
  else if(!host||!host->invoke_service)
   reply="Corpus service is unavailable.";
  else {
   memcpy(query.term,term,n);
   if(host->invoke_service("corpus.lookup",&query,sizeof query,
        &found,sizeof found,&used_lookup)!=STNLABZ_MODULE_OK||
      used_lookup!=sizeof found)
    reply="Corpus service is unavailable.";
   else if(!found.found)
    reply="Corpus: no matching knowledge record.";
   else if(!memchr(found.text,0,sizeof found.text)||
           !memchr(found.source,0,sizeof found.source)||
           !memchr(found.id,0,sizeof found.id))
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   else {
    written=snprintf(out,capacity,"%s [Corpus: %s; source: %s]",
                     found.text,found.id,found.source);
    if(written<0||(size_t)written>=capacity)
     reply="Corpus record exceeds this interface's response limit.";
    else {*used=(size_t)written+1;return STNLABZ_MODULE_OK;}
   }
  }
 }else {
  digit_interpret_request_t request_intent={0};
  digit_interpret_response_t interpreted={0};
  size_t result_used=0;
  request_intent.version=DIGIT_INTERPRETATION_VERSION;
  if(irc_private_context(c))snprintf(request_intent.text,sizeof request_intent.text,"Digit %s",s);
  else snprintf(request_intent.text,sizeof request_intent.text,"%s",s);
  /* First pass determines the lookup subject; second pass applies evidence. */
  if(host&&host->invoke_service&&
     host->invoke_service("interpretation.classify",&request_intent,sizeof request_intent,
                          &interpreted,sizeof interpreted,&result_used)==STNLABZ_MODULE_OK&&
     result_used==sizeof interpreted&&interpreted.version==DIGIT_INTERPRETATION_VERSION&&
     interpreted.intent==DIGIT_INTENT_AMBIGUOUS)
    greeting_evidence(interpreted.subject[0]?interpreted.subject:s,
                      request_intent.corpus_text,sizeof request_intent.corpus_text);
  result_used=0;
  if(!host||!host->invoke_service||
     host->invoke_service("interpretation.classify",&request_intent,sizeof request_intent,
                          &interpreted,sizeof interpreted,&result_used)!=STNLABZ_MODULE_OK||
     result_used!=sizeof interpreted||interpreted.version!=DIGIT_INTERPRETATION_VERSION)
   reply="General language interpretation is unavailable.";
  else if(interpreted.intent==DIGIT_INTENT_GREETING){
   int w=snprintf(out,capacity,"%s",interpreted.subject);
   if(w<0||(size_t)w>=capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
   *used=(size_t)w+1;append_social(interpreted.subject,out,capacity,used);
   return STNLABZ_MODULE_OK;
  }
  else if(interpreted.intent==DIGIT_INTENT_CAPABILITIES)
   reply="I can answer greetings, identify myself using Core, retrieve factual records from Corpus, and report a scoped diagnosis of your previous request. Please ask about a specific subject.";
  else if(interpreted.intent==DIGIT_INTENT_CHECKIN)
   reply="I'm here and receiving your messages. What would you like me to check?";
  else if(interpreted.intent==DIGIT_INTENT_CORPUS_GUIDANCE)
   reply="I can look up a specific subject in Corpus. What subject or record should I check?";
  else if(interpreted.intent==DIGIT_INTENT_OBSERVATION)
   reply="I received your observation. I have not verified the report against Corpus; ask me about a specific record or subject to check.";
  else if(interpreted.intent==DIGIT_INTENT_KNOWLEDGE){
   digit_corpus_lookup_request_t query={0};
   digit_corpus_lookup_response_t record={0};
   size_t n=0;int written;
   if(!memchr(interpreted.subject,0,sizeof interpreted.subject)||
      !interpreted.subject[0])return STNLABZ_MODULE_ERR_INVALID_STATE;
   snprintf(query.term,sizeof query.term,"%s",interpreted.subject);
   if(host->invoke_service("corpus.lookup",&query,sizeof query,&record,sizeof record,&n)!=STNLABZ_MODULE_OK||
      n!=sizeof record)
    reply="Corpus service is unavailable.";
   else if(!record.found)
    reply="Corpus: no matching knowledge record.";
   else if(!memchr(record.text,0,sizeof record.text)||
           !memchr(record.id,0,sizeof record.id)||
           !memchr(record.source,0,sizeof record.source))
    return STNLABZ_MODULE_ERR_INVALID_STATE;
   else {
    written=snprintf(out,capacity,"%s [Corpus: %s; source: %s]",
                      record.text,record.id,record.source);
    if(written<0||(size_t)written>=capacity)
     reply="Corpus record exceeds this interface's response limit.";
    else {*used=(size_t)written+1;return STNLABZ_MODULE_OK;}
   }
  }else if(interpreted.addressed&&c&&c->transport[0]&&c->channel[0]){
   /* Do not mask an invalid transport scope as a successful clarification. */
   if((strcmp(c->transport,"irc")&&strcmp(c->transport,"slack"))||
      (c->transport[0]=='i'&&c->channel[0]!='#'))
    return STNLABZ_MODULE_ERR_NOT_FOUND;
   stnlabz_module_result_t evaluated=contextual_answer(c,s,interpreted.subject,interpreted.addressed,response,capacity,used);
   if(evaluated==STNLABZ_MODULE_OK&&*used>1)return evaluated;
   reply="I received your message. Could you clarify what you would like me to address?";
  }else if(interpreted.addressed){
   reply="I received your message. Could you clarify what you'd like me to address?";
  }else
   return contextual_answer(c,s,interpreted.subject,0,response,capacity,used);
 }
 if(strlen(reply)+1>capacity)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(out,reply,strlen(reply)+1);*used=strlen(reply)+1;
 /* Never decorate factual answers, errors or diagnostics. */
 if(!strcasecmp(s,"hello")||!strcasecmp(s,"hi")||
    !strcasecmp(s,"hello digit")||!strcasecmp(s,"hi digit")||
    !strcasecmp(s,"hello digit!")||!strcasecmp(s,"hi digit!")||
    !strcmp(reply,"I'm here and receiving your messages. What would you like me to check?"))
  append_social(s,out,capacity,used);
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);q->tests_executed=q->tests_passed=STNLABZ_MODULE_MIN_TESTS;
 q->negative_test_executed=q->negative_test_passed=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(host||!h||!h->register_service||!h->unregister_service)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!h->register_service("route./ask",answer,NULL))return STNLABZ_MODULE_ERR_START_FAILED;
 host=h;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!host)return STNLABZ_MODULE_ERR_INVALID_STATE;
 if(!host->unregister_service("route./ask",NULL))return STNLABZ_MODULE_ERR_STOP_FAILED;
 host=NULL;return STNLABZ_MODULE_OK;
}
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 static stnlabz_module_descriptor_t d;
 memset(&d,0,sizeof d);snprintf(d.id,sizeof d.id,"ask");snprintf(d.name,sizeof d.name,"Digit Ask");
 d.version_major=0;/* [AI:GPT-6 | 2026-10-17 02:49:54 UTC] */
 d.version_minor=3;d.version_patch=2;
 /* [End AI:GPT-6] */
 d.required_core_api_major=STNLABZ_MODULE_API_MAJOR;d.required_core_api_minor=STNLABZ_MODULE_API_MINOR;
 d.qualify=qualify;d.start=start;d.stop=stop;return &d;
}
