#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include "corpus_store.h"
static int copy(char *dst,size_t cap,const char *src){
 size_t n=strlen(src);
 if(!n||n>=cap)return 0;
 memcpy(dst,src,n+1);return 1;
}
static int parse(char *line,digit_corpus_record_t *r){
 char *fields[4],*p=line;size_t i;
 for(i=0;i<3;i++){
  char *tab=strchr(p,'\t');
  if(!tab)return 0;
  *tab=0;fields[i]=p;p=tab+1;
 }
 fields[3]=p;
 if(strchr(p,'\t'))return 0;
 p+=strlen(p);
 while(p>fields[3]&&(p[-1]=='\n'||p[-1]=='\r'))*--p=0;
 return copy(r->id,sizeof r->id,fields[0])&&
        copy(r->category,sizeof r->category,fields[1])&&
        copy(r->source,sizeof r->source,fields[2])&&
        copy(r->text,sizeof r->text,fields[3]);
}
static int word(unsigned char c){return isalnum(c)||c=='_'||c=='-';}
static int exact_token(const char *text,const char *term){
 size_t n=strlen(term),i,j;
 for(i=0;text[i];i++){
  if(i&&word((unsigned char)text[i-1]))continue;
  for(j=0;j<n&&text[i+j];j++)
   if(tolower((unsigned char)text[i+j])!=tolower((unsigned char)term[j]))break;
  if(j==n&&!word((unsigned char)text[i+j]))return 1;
 }
 return 0;
}
/* Ranking is positional and evidence-based, not a subject-specific exception.
 * Prefer records whose text begins with the requested subject. */
static unsigned match_rank(const char *text,const char *term){
 size_t n=strlen(term);
 if(!exact_token(text,term))return 0;
 if(!strncasecmp(text,term,n) && !word((unsigned char)text[n]))
  return 3;
 return 1;
}
int digit_corpus_find(const char *path,const char *term,digit_corpus_record_t *out){
 FILE *f;char *line=NULL;size_t cap=0;ssize_t got;
 digit_corpus_record_t r;int found=0;unsigned best=0;
 if(!path||!term||!out||!term[0]||strlen(term)>128||
    strchr(term,'\n')||strchr(term,'\r')||strchr(term,'\t'))return -2;
 memset(out,0,sizeof *out);
 f=fopen(path,"r");if(!f)return -1;
 while((got=getline(&line,&cap,f))>=0){
  (void)got;
  memset(&r,0,sizeof r);
  if(!parse(line,&r))continue;
  /* Prefer an exact record id; otherwise only a token/phrase in knowledge text. */
  if(!strcasecmp(r.id,term)){*out=r;found=1;break;}
  {unsigned score=match_rank(r.text,term);
   if(score>best){*out=r;found=1;best=score;}
  }
 }
 free(line);
 if(ferror(f)){fclose(f);return -1;}
 fclose(f);return found;
}

/* Read at most three distinct records matching a token or phrase.
 * A record ID match is ranked above a subject-leading text match;
 * equal scores preserve file order. No corpus changes or synthesized facts. */
int digit_corpus_find_multi(const char *path,const char *term,
 digit_corpus_record_t out[3]){
 FILE *f;char *line=NULL;size_t cap=0;ssize_t got;
 digit_corpus_record_t r;unsigned ranks[3]={0};size_t count=0,i,slot;
 if(!path||!term||!out||!term[0]||strlen(term)>128||
    strchr(term,'\n')||strchr(term,'\r')||strchr(term,'\t'))return -2;
 memset(out,0,3*sizeof *out);
 f=fopen(path,"r");if(!f)return -1;
 while((got=getline(&line,&cap,f))>=0){
  unsigned rank; (void)got;
  memset(&r,0,sizeof r);if(!parse(line,&r))continue;
  rank=!strcasecmp(r.id,term)?4:match_rank(r.text,term);
  if(!rank)continue;
  for(i=0;i<count;i++)if(!strcmp(out[i].id,r.id))break;
  if(i<count)continue;
  slot=0;
  while(slot<count&&ranks[slot]>=rank)slot++;
  if(slot>=3)continue;
  if(count<3)count++;
  for(i=count-1;i>slot;i--){out[i]=out[i-1];ranks[i]=ranks[i-1];}
  out[slot]=r;ranks[slot]=rank;
 }
 free(line);
 if(ferror(f)){fclose(f);return -1;}
 fclose(f);return (int)count;
}

/* Every count below is backed by an executed assertion; production data is untouched. */
stnlabz_module_result_t digit_corpus_selftest(stnlabz_module_qualification_result_t *q){
 char name[]="/tmp/digit-qual-XXXXXX";
 digit_corpus_record_t r;
 FILE *f;
 int fd,rc;
 unsigned int passed=0,negative=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 fd=mkstemp(name);
 if(fd<0)return STNLABZ_MODULE_ERR_QUALIFICATION;
 f=fdopen(fd,"w");
 if(!f){close(fd);unlink(name);return STNLABZ_MODULE_ERR_QUALIFICATION;}
 if(fputs("Q-01\tRULE\tqualification\tThe cobalt engine remains stable.\n"
          "Q-02\tENGINEERING\ttest-source\tA quiet engine emits a tone.\n"
          "bad line without tabs\n",f)==EOF||fclose(f)!=0){
  /* A failed fixture is a failed qualification; never report PASS. */
  unlink(name);return STNLABZ_MODULE_ERR_QUALIFICATION;
 }
#define CHECK(v) do{q->tests_executed++;if(v)passed++;else q->tests_failed++;}while(0)
 rc=digit_corpus_find(name,"cobalt",&r);
 CHECK(rc==1);
 CHECK(rc==1&&!strcmp(r.id,"Q-01"));
 CHECK(rc==1&&!strcmp(r.category,"RULE"));
 CHECK(rc==1&&!strcmp(r.source,"qualification"));
 CHECK(rc==1&&!strcmp(r.text,"The cobalt engine remains stable."));
 rc=digit_corpus_find(name,"Q-02",&r);
 CHECK(rc==1&&!strcmp(r.id,"Q-02"));
 CHECK(rc==1&&!strcmp(r.source,"test-source"));
 rc=digit_corpus_find(name,"ENGINE",&r);
 CHECK(rc==1&&!strcmp(r.id,"Q-01"));
 rc=digit_corpus_find(name,"cobal",&r);
 CHECK(rc==0);
 negative=(unsigned int)(rc==0);
 rc=digit_corpus_find(name,"ABSENT",&r);
 CHECK(rc==0);
 rc=digit_corpus_find(name,"",&r);
 CHECK(rc==-2);
 rc=digit_corpus_find(name,"bad\tquery",&r);
 CHECK(rc==-2);
 unlink(name);
 rc=digit_corpus_find(name,"cobalt",&r);
 CHECK(rc==-1);
 q->tests_passed=passed;
 q->negative_test_executed=1;
 q->negative_test_passed=(int)negative;
#undef CHECK
 return q->tests_executed>=STNLABZ_MODULE_MIN_TESTS&&q->tests_failed==0&&
 q->negative_test_passed?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
