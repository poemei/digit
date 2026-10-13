#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "corpus_store.h"
int main(void){
 char path[]="/tmp/digit-corpus-XXXXXX";
 digit_corpus_record_t r;
 FILE *f;int fd=mkstemp(path),failed=0;
 if(fd<0)return 2;
 f=fdopen(fd,"w");if(!f){close(fd);unlink(path);return 2;}
 fputs("TEST-1\tRULE\toperator\tEngine records must remain intact.\n",f);
 fputs("TEST-2\tENGINEERING\ttest\tThe feline makes a purrs sound.\n",f);
 fclose(f);
 if(digit_corpus_find(path,"purrs",&r)!=1||strcmp(r.id,"TEST-2")||
    strcmp(r.source,"test")){puts("FAIL corpus lookup/source");failed++;}
 if(digit_corpus_find(path,"purr",&r)!=0){puts("FAIL token boundaries");failed++;}
 if(digit_corpus_find(path,"TEST-1",&r)!=1||
    strcmp(r.category,"RULE")){puts("FAIL id lookup");failed++;}
 if(digit_corpus_find(path,"absent",&r)!=0){puts("FAIL unknown");failed++;}
 if(digit_corpus_find("/this/path/does/not/exist","purrs",&r)!=-1){
  puts("FAIL unavailable Corpus");failed++;
 }
 unlink(path);
 printf("Corpus store regression: %d failed\n",failed);
 return failed?1:0;
}
