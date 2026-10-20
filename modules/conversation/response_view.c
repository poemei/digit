/* [AI:GPT-6 | 2026-10-20] View-owned source-faithful presentation.
 * A bounded excerpt is a quotation, NOT an authored explanation. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "response_view.h"

/* A capitalization follow-up concerns the orthography of a named term,
 * not the source's entire definition. Keep the user's original casing. */
static int spelling_followup(const char *text,char *term,size_t capacity){
 const char *p=text;size_t n=0;
 static const char prefix[]="or is it ";
 size_t i;
 if(!text||!term||capacity<2)return 0;
 while(*p&&isspace((unsigned char)*p))p++;
 for(i=0;i<sizeof prefix-1;i++)
  if(!p[i]||tolower((unsigned char)p[i])!=prefix[i])return 0;
 p+=sizeof prefix-1;
 while(*p&&isspace((unsigned char)*p))p++;
 while(isalnum((unsigned char)p[n])||p[n]=='_'||p[n]=='-'){
  if(n+1>=capacity)return 0;
  term[n]=p[n];n++;
 }
 if(!n)return 0;
 term[n]=0;p+=n;
 while(*p&&isspace((unsigned char)*p))p++;
 return !*p||(*p=='?'&&!p[1]);
}
static int corpus_spelling(const char *source,const char *term,
 char *spelling,size_t capacity){
 size_t i,j,n=strlen(term);
 if(!n||n>=capacity)return 0;
 for(i=0;source[i];i++){
  if(i&&isalnum((unsigned char)source[i-1]))continue;
  for(j=0;j<n&&source[i+j]&&
      tolower((unsigned char)source[i+j])==
      tolower((unsigned char)term[j]);j++);
  if(j==n&&!isalnum((unsigned char)source[i+j])){
   memcpy(spelling,source+i,n);spelling[n]=0;return 1;
  }
 }
 return 0;
}

int digit_response_render(const digit_reasoning_result_t *d,
 const digit_corpus_result_t *corpus,const digit_interpretation_result_t *question,
 const char *original_text,char *output,size_t capacity,size_t *used){
 unsigned i,j;const digit_corpus_hit_t *hit=NULL;
 size_t length=0,end=0,source_length,words=0;
 int inword=0,has_detail=0;
 int n;
 if(used)*used=0;
 if(!d||!corpus||!question||!original_text||!output||!used||capacity<2)return 0;
 if(d->outcome==DIGIT_REASONING_OUTCOME_UNKNOWN||
    d->outcome==DIGIT_REASONING_OUTCOME_CLARIFY||
    d->outcome==DIGIT_REASONING_OUTCOME_FAILURE){
  const char *message=d->outcome==DIGIT_REASONING_OUTCOME_UNKNOWN?
    "I don't know based on the available evidence.":
    d->outcome==DIGIT_REASONING_OUTCOME_CLARIFY?
    "I need clarification before answering.":
    "Evidence retrieval failed. I cannot verify an answer.";
  n=snprintf(output,capacity,"%s",message);
  if(n<=0||(size_t)n>=capacity)return 0;
  *used=(size_t)n;return 1;
 }
 if((d->outcome!=DIGIT_REASONING_OUTCOME_EVIDENCE&&
     d->outcome!=DIGIT_REASONING_OUTCOME_PARTIAL)||
    d->requires_verification||!d->reference_count||
    d->reference_count>DIGIT_REASONING_MAX_REFERENCES||
    corpus->count>DIGIT_CORPUS_RESULTS_MAX)return 0;
 for(i=0;i<d->reference_count&&!hit;i++)
  for(j=0;j<corpus->count;j++)
   if(d->corpus_ids[i]==corpus->hits[j].id&&
      corpus->hits[j].id>0&&
      (!strcmp(corpus->hits[j].verification_status,"operator-verified")||
       !strcmp(corpus->hits[j].verification_status,"verified"))){
       hit=&corpus->hits[j];break;
   }
 if(!hit||!memchr(hit->excerpt,0,sizeof hit->excerpt)||!hit->excerpt[0])
  return 0;
 {char asked[48],stored[48];
  if(spelling_followup(original_text,asked,sizeof asked)&&
     corpus_spelling(hit->excerpt,asked,stored,sizeof stored)){
   n=snprintf(output,capacity,
     "Corpus #%lld writes it as \"%s\". This source does not establish "
     "whether that capitalization is required.",
     (long long)hit->id,stored);
   if(n<=0||(size_t)n>=capacity)return 0;
   *used=(size_t)n;return 1;
  }
 }
 /* A source discussing an activity is not proof Digit can perform it. */
 if(question->term_count>0&&
    (!strcmp(question->terms[0],"can")||
     !strcmp(question->terms[0],"could"))){
  unsigned k;int self=0;
  for(k=1;k<question->term_count;k++)
   if(!strcmp(question->terms[k],"you")||
      !strcmp(question->terms[k],"digit")||
      !strcmp(question->terms[k],"digicat"))self=1;
  if(self){
   n=snprintf(output,capacity,
     "I have verified Corpus knowledge related to your question, "
     "but that evidence alone doesn't establish that I can do it.");
   if(n<=0||(size_t)n>=capacity)return 0;
   *used=(size_t)n;return 1;
  }
 }
 source_length=strlen(hit->excerpt);
 for(i=0;i<source_length;i++){
  unsigned char c=(unsigned char)hit->excerpt[i];
  if(isalpha(c)){if(!inword){words++;inword=1;}}
  else {inword=0;if(c=='.'||c==';'||c==':')has_detail=1;}
 }
 if(d->outcome==DIGIT_REASONING_OUTCOME_EVIDENCE&&
    (words<6||!has_detail))return 0;
 if(d->outcome==DIGIT_REASONING_OUTCOME_PARTIAL&&words<4)return 0;
 for(end=0;end<source_length&&end<220;end++){
  unsigned char c=(unsigned char)hit->excerpt[end];
  if(c<32||c==127)return 0;
  if(c=='.'||c=='!'||c=='?'){end++;break;}
 }
 if(end==0)return 0;
 /* Never present a mid-word cut without indicating excerpt truncation. */
 if(end<source_length&&end==220){
  while(end>0&&hit->excerpt[end-1]!=' ')end--;
  while(end>0&&hit->excerpt[end-1]==' ')end--;
 }
 if(!end)return 0;
 n=snprintf(output,capacity,
   d->outcome==DIGIT_REASONING_OUTCOME_PARTIAL?
     "What I can establish from Corpus #%lld (not a full explanation): \"%.*s%s\"":
     "Corpus #%lld excerpt: \"%.*s%s\"",
   (long long)hit->id,(int)end,hit->excerpt,
   end<source_length?"...":"");
 if(n<=0||(size_t)n>=capacity)return 0;
 length=(size_t)n;
 *used=length;
 return 1;
}
