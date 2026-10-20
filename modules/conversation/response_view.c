/* [AI:GPT-6 | 2026-10-20] View-owned grounded response presentation.
 * Renders only source-provided evidence selected by Reasoning. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "response_view.h"

int digit_response_render(const digit_reasoning_result_t *d,
 const digit_corpus_result_t *corpus,char *output,size_t capacity,size_t *used){
 unsigned i,j;const digit_corpus_hit_t *hit=NULL;
 size_t length=0,end;
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
      !strcmp(corpus->hits[j].verification_status,"operator-verified"))
       {hit=&corpus->hits[j];break;}
 if(!hit||!memchr(hit->excerpt,0,sizeof hit->excerpt)||!hit->excerpt[0])
  return 0;
 /* Extract the first bounded statement from evidence. Never fabricate a
  * conclusion or present a truncated sentence as complete. */
 for(end=0;hit->excerpt[end];end++){
  unsigned char c=(unsigned char)hit->excerpt[end];
  if(c=='.'||c=='!'||c=='?'){end++;break;}
 }
 if(!end||end>240||hit->excerpt[end-1]!='.'&&
    hit->excerpt[end-1]!='!'&&hit->excerpt[end-1]!='?')return 0;
 for(i=0;i<end;i++)
  if((unsigned char)hit->excerpt[i]<32||
     (unsigned char)hit->excerpt[i]==127)return 0;
 n=snprintf(output,capacity,"Corpus #%lld: %.*s",
   (long long)hit->id,(int)end,hit->excerpt);
 if(n<=0||(size_t)n>=capacity)return 0;
 length=(size_t)n;
 *used=length;
 return 1;
}
