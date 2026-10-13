#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
int digit_corpus_find(const char *path,const char *term,digit_corpus_record_t *out){
 FILE *f;char *line=NULL;size_t cap=0;ssize_t got;
 digit_corpus_record_t r;int found=0;
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
  if(!found&&exact_token(r.text,term)){*out=r;found=1;}
 }
 free(line);
 if(ferror(f)){fclose(f);return -1;}
 fclose(f);return found;
}
