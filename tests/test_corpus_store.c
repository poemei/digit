#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "corpus_store.h"
int main(void){
 stnlabz_module_qualification_result_t qual;
 stnlabz_module_result_t qualification=digit_corpus_selftest(&qual);
 int qok=qualification==STNLABZ_MODULE_OK&&qual.tests_executed>=10&&
 qual.tests_passed==qual.tests_executed&&qual.tests_failed==0&&
 qual.negative_test_executed&&qual.negative_test_passed;
 if(!qok){fputs("FAIL executable qualification\n",stderr);return 1;}
 if(digit_corpus_selftest(NULL)!=STNLABZ_MODULE_ERR_INVALID_ARGUMENT){
  fputs("FAIL qualification null-argument rejection\n",stderr);return 1;
 }
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
 printf("Corpus store tests: 7 executed, %d failed\n",failed);
 return failed?1:0;
}
