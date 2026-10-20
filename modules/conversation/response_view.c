/* [AI:GPT-6 | 2026-10-20] View-owned source-faithful presentation.
 * A bounded excerpt is a quotation, NOT an authored explanation. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "response_view.h"

int digit_response_render(const digit_reasoning_result_t *d,
 const digit_corpus_result_t *corpus,char *output,size_t capacity,size_t *used){
 unsigned i,j;const digit_corpus_hit_t *hit=NULL;
 size_t length=0,end=0,source_length;
 int n;
 if(used)*used=0;
 if(!d||!corpus||!output||!used||capacity<2||
    d->outcome!=DIGIT_REASONING_OUTCOME_EVIDENCE||
    d->requires_verification||!d->reference_count||
    d->reference_count>DIGIT_REASONING_MAX_REFERENCES||
    corpus->count>DIGIT_CORPUS_RESULTS_MAX)return 0;
 for(i=0;i<d->reference_count&&!hit;i++)
  for(j=0;j<corpus->count;j++)
   if(d->corpus_ids[i]==corpus->hits[j].id&&
      corpus->hits[j].id>0&&
      !strcmp(corpus->hits[j].verification_status,"operator-verified")){
       hit=&corpus->hits[j];break;
   }
 if(!hit||!memchr(hit->excerpt,0,sizeof hit->excerpt)||!hit->excerpt[0])
  return 0;
 source_length=strlen(hit->excerpt);
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
 n=snprintf(output,capacity,"Corpus #%lld excerpt: \"%.*s%s\"",
   (long long)hit->id,(int)end,hit->excerpt,
   end<source_length?"...":"");
 if(n<=0||(size_t)n>=capacity)return 0;
 length=(size_t)n;
 *used=length;
 return 1;
}
