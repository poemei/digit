#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "research_internal.h"

static const char *const topics[]={
 "security","autonomous-systems","software-engineering","space-systems"
};
static int equals_folded(const char *a,const char *b)
{
 unsigned char x,y;
 if(!a||!b)return 0;
 for(;*a&&*b;a++,b++){
  x=(unsigned char)*a;y=(unsigned char)*b;
  if(tolower(x)!=tolower(y))return 0;
 }
 return !*a&&!*b;
}
int research_topic_valid(const char *s)
{
 size_t i;
 if(!s)return 0;
 for(i=0;i<sizeof(topics)/sizeof(topics[0]);i++)
  if(equals_folded(s,topics[i]))return 1;
 return 0;
}
int research_parse(const char *args,char *topic,size_t tc,char *terms,size_t qc)
{
 const char *p,*sep;size_t n,k;
 if(!args||!topic||!terms||tc<2||qc<2)return 0;
 p=args;
 while(*p==' ')p++;
 sep=strchr(p,' ');
 if(!sep)return 0;
 n=(size_t)(sep-p);
 if(!n||n>=tc)return 0;
 memcpy(topic,p,n);topic[n]=0;
 if(!research_topic_valid(topic))return 0;
 p=sep;
 while(*p==' ')p++;
 k=strlen(p);
 if(k==0||k>=qc)return 0;
 for(n=0;n<k;n++) {
  unsigned char ch=(unsigned char)p[n];
  if(ch<32||ch==127)return 0;
 }
 memcpy(terms,p,k+1);
 return 1;
}
static int contains_folded(const char *line,const char *query)
{
 size_t i,j,ql=strlen(query),len=strlen(line);
 if(!ql||ql>len)return 0;
 for(i=0;i+ql<=len;i++){
  for(j=0;j<ql;j++)
   if(tolower((unsigned char)line[i+j])!=tolower((unsigned char)query[j]))break;
  if(j==ql)return 1;
 }
 return 0;
}
int research_search(const char *root,const char *topic,const char *terms,
 char *answer,size_t capacity)
{
 char directory[512],path[768],line[2048]; DIR *dir;
 struct dirent *entry; struct stat st;
 int found=0;
 if(!root||!topic||!terms||!answer||capacity<64||
    !research_topic_valid(topic)||!terms[0])return 0;
 if(snprintf(directory,sizeof directory,"%s/%s",root,topic)>=(int)sizeof directory)return 0;
 dir=opendir(directory);
 if(!dir){
  (void)snprintf(answer,capacity,"No local research evidence found; status UNKNOWN.");
  return 1;
 }
 while((entry=readdir(dir))!=NULL){
  size_t size=strlen(entry->d_name);
  FILE *f;
  if(size<5||strcmp(entry->d_name+size-4,".txt")||
    entry->d_name[0]=='.'||strchr(entry->d_name,'/'))continue;
  if(snprintf(path,sizeof path,"%s/%s",directory,entry->d_name)>=(int)sizeof path)continue;
  if(lstat(path,&st)!=0||!S_ISREG(st.st_mode)||st.st_size>1048576)continue;
  f=fopen(path,"r");
  if(!f)continue;
  while(fgets(line,sizeof line,f)){
   size_t n;
   if(!strchr(line,'\n')&&!feof(f)){int c;while((c=fgetc(f))!='\n'&&c!=EOF){}continue;}
   if(!contains_folded(line,terms))continue;
   n=strcspn(line,"\r\n");line[n]=0;
   if(snprintf(answer,capacity,"Source %s: %.220s [UNVERIFIED; human review required]",
     entry->d_name,line)<(int)capacity)found=1;
   break;
  }
  fclose(f);
  if(found)break;
 }
 closedir(dir);
 if(!found)(void)snprintf(answer,capacity,"No matching local research evidence; status UNKNOWN.");
 return 1;
}
