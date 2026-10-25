#define _POSIX_C_SOURCE 200809L
/*
 * [AI:GPT-6 | 2026-10-25 18:25:00 UTC]
 * ST-1 native research module for Digit ABI 1.5. Independent read-only
 * services and Router commands; local authorized CLI owns writes.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "st1.h"
#ifndef ST1_PATH
#define ST1_PATH DIGIT_ST1_PATH
#endif
#ifndef O_NOFOLLOW
#error "ST-1 storage requires O_NOFOLLOW"
#endif
#define ST1_LINE 2048
#define ST1_FIELDS 13
#ifndef ST1_CLI
static const stnlabz_module_host_t *module_host;
#endif

static int bounded(const char *p,size_t n,int required){
 size_t i;
 if(!p)return 0;
 for(i=0;i<n;i++){
  unsigned char c=(unsigned char)p[i];
  if(!c)return !required||i>0;
  if(c<32||c==127)return 0;
 }
 return 0;
}
static int oneof(const char *s,const char *a,const char *b,const char *c,const char *d){
 return !strcmp(s,a)||!strcmp(s,b)||!strcmp(s,c)||(d&&!strcmp(s,d));
}
static int date_ok(const char *s){
 static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
 unsigned y,m,d;size_t i;
 if(!s||strlen(s)!=10||s[4]!='-'||s[7]!='-')return 0;
 for(i=0;i<10;i++)if(i!=4&&i!=7&&!isdigit((unsigned char)s[i]))return 0;
 y=(unsigned)(s[0]-'0')*1000u+(unsigned)(s[1]-'0')*100u+
   (unsigned)(s[2]-'0')*10u+(unsigned)(s[3]-'0');
 m=(unsigned)(s[5]-'0')*10u+(unsigned)(s[6]-'0');
 d=(unsigned)(s[8]-'0')*10u+(unsigned)(s[9]-'0');
 return y&&m>=1&&m<=12&&d>=1&&
        d<=days[m-1]+(unsigned)(m==2&&y%4==0&&(y%100!=0||y%400==0));
}
static int valid(const digit_st1_record_t *r){
 size_t i;
 if(!r||!bounded(r->id,sizeof r->id,1)||
    !bounded(r->kind,sizeof r->kind,1)||
    !bounded(r->title,sizeof r->title,1)||
    !bounded(r->url,sizeof r->url,1)||
    !bounded(r->date,sizeof r->date,1)||
    !bounded(r->summary,sizeof r->summary,1)||
    !bounded(r->evidence,sizeof r->evidence,1)||
    !bounded(r->relevance,sizeof r->relevance,1)||
    !bounded(r->systems,sizeof r->systems,1)||
    !bounded(r->impact,sizeof r->impact,1)||
    !bounded(r->milestone,sizeof r->milestone,0))return 0;
 for(i=0;r->id[i];i++)
  if(!isalnum((unsigned char)r->id[i])&&r->id[i]!='-'&&r->id[i]!='_')return 0;
 return oneof(r->kind,"PAPER","REPORT","STANDARD","MISSION")&&
        !strncmp(r->url,"https://",8)&&strlen(r->url)>8&&date_ok(r->date)&&
        oneof(r->evidence,"Verified","Reported","Inferred","Unknown")&&
        oneof(r->relevance,"High","Medium","Low",NULL)&&
        oneof(r->impact,"Reinforces","Challenges","PotentialChange","None")&&
        (strcmp(r->kind,"MISSION")||r->milestone[0]);
}
static int parse(char *line,digit_st1_record_t *r){
 char *part[ST1_FIELDS],*p;size_t n=1;unsigned long rev;
 if(!line||!r)return 0;
 p=strchr(line,'\n');if(!p)return 0;*p=0;
 part[0]=line;
 for(p=line;*p;p++)if(*p=='\t'){
  if(n>=ST1_FIELDS)return 0;
  *p=0;part[n++]=p+1;
 }
 if(n!=ST1_FIELDS)return 0;
 memset(r,0,sizeof(*r));
#define FIELD(F,I) do{if(strlen(part[I])>=sizeof(r->F))return 0;strcpy(r->F,part[I]);}while(0)
 FIELD(id,0);FIELD(kind,1);FIELD(title,2);FIELD(url,3);
 FIELD(date,4);FIELD(summary,5);FIELD(evidence,6);FIELD(relevance,7);
 FIELD(systems,8);FIELD(impact,9);FIELD(milestone,10);FIELD(recorded,12);
#undef FIELD
 errno=0;rev=strtoul(part[11],&p,10);
 if(errno||!part[11][0]||*p||!rev||rev>UINT32_MAX)return 0;
 r->revision=(unsigned int)rev;
 return valid(r)&&strlen(r->recorded)==20;
}
/* Shared read-only query engine for native ABI and operator CLI. */
static int contains_ci(const char *text,const char *term){
 size_t i,j,n=strlen(term),len=strlen(text);
 if(!n||n>len)return 0;
 for(i=0;i+n<=len;i++){
  for(j=0;j<n;j++)
   if(tolower((unsigned char)text[i+j])!=tolower((unsigned char)term[j]))break;
  if(j==n)return 1;
 }
 return 0;
}
static int matches(const digit_st1_record_t *r,const digit_st1_query_t *q){
 if(!strcmp(q->action,"list"))return 1;
 if(!strcmp(q->action,"watchlist"))return !strcmp(r->kind,"MISSION");
 if(!strcmp(q->action,"show")||!strcmp(q->action,"history"))
  return !strcmp(r->id,q->term);
 if(!strcmp(q->action,"search"))
  return contains_ci(r->id,q->term)||contains_ci(r->kind,q->term)||
   contains_ci(r->title,q->term)||contains_ci(r->url,q->term)||
   contains_ci(r->summary,q->term)||contains_ci(r->systems,q->term)||
   contains_ci(r->milestone,q->term);
 return 0;
}
static int query(const char *path,const digit_st1_query_t *q,digit_st1_query_result_t *out){
 FILE *f;int fd;struct stat st;char line[ST1_LINE];digit_st1_record_t r;unsigned i;
 if(!path||!q||!out||!bounded(q->action,sizeof q->action,1)||
    !bounded(q->term,sizeof q->term,0))return 0;
 if(!oneof(q->action,"list","watchlist","show","history")&&strcmp(q->action,"search"))return 0;
 if((!strcmp(q->action,"search")||!strcmp(q->action,"history")||
     !strcmp(q->action,"show"))&&!q->term[0])return 0;
 memset(out,0,sizeof(*out));
 fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0){out->error=errno==ENOENT?1:2;return 0;}
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||(st.st_mode&0007)){
  (void)close(fd);out->error=2;return 0;
 }
 f=fdopen(fd,"r");if(!f){close(fd);out->error=2;return 0;}
 while(fgets(line,sizeof line,f)){
  if(!strchr(line,'\n')||!parse(line,&r)){out->error=3;break;}
  if(!matches(&r,q))continue;
  if(strcmp(q->action,"history")){
   for(i=0;i<out->count;i++)if(!strcmp(r.id,out->records[i].id))break;
   if(i<out->count){
    if(r.revision>out->records[i].revision)out->records[i]=r;
    continue;
   }
  }
  if(out->count<DIGIT_ST1_RESULTS)out->records[out->count++]=r;
 }
 if(ferror(f))out->error=4;
 if(fclose(f))out->error=4;
 return !out->error;
}

#ifdef ST1_CLI
static int append_record(const char *path,digit_st1_record_t *r){
 int fd,ok=0;struct flock lock;struct stat st;
 char line[ST1_LINE],buf[ST1_LINE];ssize_t n;unsigned next=1;
 if(!valid(r))return 0;
 fd=open(path,O_RDWR|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);
 if(fd<0)return 0;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_nlink!=1||(st.st_mode&0077))goto done;
 memset(&lock,0,sizeof lock);lock.l_type=F_WRLCK;lock.l_whence=SEEK_SET;
 if(fcntl(fd,F_SETLKW,&lock))goto done;
 if(lseek(fd,0,SEEK_SET)<0)goto unlock;
 {
  size_t used=0;
  while((n=read(fd,buf+used,sizeof buf-used))>0){
   size_t pos=0,total=used+(size_t)n;
   while(pos<total){
    char *nl=memchr(buf+pos,'\n',total-pos);
    size_t len;
    digit_st1_record_t old;
    if(!nl)break;
    len=(size_t)(nl-(buf+pos))+1;
    if(len>=sizeof line)goto unlock;
    memcpy(line,buf+pos,len);line[len]=0;
    if(!parse(line,&old))goto unlock;
    if(!strcmp(old.id,r->id)&&old.revision>=next){
     if(old.revision==UINT32_MAX)goto unlock;
     next=old.revision+1;
    }
    pos+=len;
   }
   used=total-pos;
   memmove(buf,buf+pos,used);
   if(used==sizeof buf)goto unlock;
  }
  if(n<0||used)goto unlock;
 }
 r->revision=next;
 {
  time_t now=time(NULL);struct tm value;
  if(now==(time_t)-1||!gmtime_r(&now,&value)||
     !strftime(r->recorded,sizeof r->recorded,"%Y-%m-%dT%H:%M:%SZ",&value))
   goto unlock;
 }
 {
  int count=snprintf(line,sizeof line,
   "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%s\n",
   r->id,r->kind,r->title,r->url,r->date,r->summary,r->evidence,
   r->relevance,r->systems,r->impact,r->milestone,r->revision,r->recorded);
  if(count<0||(size_t)count>=sizeof line)goto unlock;
  if(write(fd,line,(size_t)count)!=count)goto unlock;
  if(fsync(fd))goto unlock;
 }
 ok=1;
unlock:
 lock.l_type=F_UNLCK;(void)fcntl(fd,F_SETLK,&lock);
done:
 (void)close(fd);return ok;
}
int main(int argc,char **argv){
 digit_st1_record_t r;
 if(((argc==2||argc==3)&&oneof(argv[1],"list","watchlist","show","history")) ||
    (argc==3&&!strcmp(argv[1],"search"))){
  digit_st1_query_t q={0};
  digit_st1_query_result_t out;
  unsigned i;
  if(strlen(argv[1])>=sizeof q.action||
     (argc==3&&strlen(argv[2])>=sizeof q.term))return 2;
  strcpy(q.action,argv[1]);
  if(argc==3)strcpy(q.term,argv[2]);
  if(!query(ST1_PATH,&q,&out)){
   fputs("st1: registry unavailable or query rejected\n",stderr);return 1;
  }
  for(i=0;i<out.count;i++){
   const digit_st1_record_t *v=&out.records[i];
   printf("%s r%u [%s/%s] %s | %s | %s\n",v->id,v->revision,
          v->evidence,v->relevance,v->title,v->date,v->url);
   if(!strcmp(q.action,"show")||!strcmp(q.action,"history"))
    printf("  %s | %s | %s | %s\n",v->systems,v->impact,v->summary,v->milestone);
  }
  return 0;
 }
 if(argc!=13||strcmp(argv[1],"upsert")){
  fputs("usage: st1_manage upsert ID KIND TITLE HTTPS_URL YYYY-MM-DD SUMMARY EVIDENCE RELEVANCE SYSTEMS IMPACT MILESTONE\n",stderr);
  return 2;
 }
 memset(&r,0,sizeof r);
#define ARG(F,I) do{if(strlen(argv[I])>=sizeof(r.F))return 2;strcpy(r.F,argv[I]);}while(0)
 ARG(id,2);ARG(kind,3);ARG(title,4);ARG(url,5);ARG(date,6);
 ARG(summary,7);ARG(evidence,8);ARG(relevance,9);
 ARG(systems,10);ARG(impact,11);ARG(milestone,12);
#undef ARG
 if(!append_record(ST1_PATH,&r)){
  fputs("st1: rejected or unable to persist record\n",stderr);return 1;
 }
 printf("st1: recorded id=%s revision=%u\n",r.id,r.revision);
 return 0;
}
#else
static stnlabz_module_result_t query_service(const void *in,size_t n,void *out,
 size_t cap,size_t *used,void *ctx){
 const digit_st1_query_t *req=in;digit_st1_query_result_t result;(void)ctx;
 if(!req||n!=sizeof(*req)||!out||!used||cap<sizeof result)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!query(ST1_PATH,req,&result))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memcpy(out,&result,sizeof result);*used=sizeof result;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 digit_st1_record_t r={0};unsigned pass=0;
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof(*q));
 strcpy(r.id,"ST1-1");strcpy(r.kind,"MISSION");
 strcpy(r.title,"Relay demo");strcpy(r.url,"https://example.org/report");
 strcpy(r.date,"2026-10-25");strcpy(r.summary,"Reported demonstration");
 strcpy(r.evidence,"Reported");strcpy(r.relevance,"Medium");
 strcpy(r.systems,"communications");strcpy(r.impact,"None");
 strcpy(r.milestone,"Published");
 pass+=valid(&r);                                       /* 1 */
 pass+=date_ok(r.date);                                 /* 2 */
 pass+=contains_ci(r.summary,"REPORTED");               /* 3 */
 pass+=!contains_ci(r.summary,"unrelated");             /* 4 */
 pass+=oneof(r.evidence,"Verified","Reported","Inferred","Unknown"); /* 5 */
 strcpy(r.evidence,"Unknown");pass+=valid(&r);           /* 6 */
 strcpy(r.evidence,"Inferred");pass+=valid(&r);          /* 7 */
 strcpy(r.evidence,"Verified");pass+=valid(&r);          /* 8 */
 strcpy(r.evidence,"INVALID");pass+=!valid(&r);          /* 9 */
 strcpy(r.evidence,"Reported");
 strcpy(r.relevance,"High");pass+=valid(&r);             /* 10 */
 strcpy(r.relevance,"Low");pass+=valid(&r);              /* 11 */
 strcpy(r.relevance,"INVALID");pass+=!valid(&r);         /* 12 */
 strcpy(r.relevance,"Medium");
 strcpy(r.url,"http://example.org");pass+=!valid(&r);   /* 13 */
 strcpy(r.url,"https://example.org/report");
 strcpy(r.id,"../escape");pass+=!valid(&r);              /* 14 */
 strcpy(r.id,"ST1-1");
 strcpy(r.date,"2026-99-99");pass+=!valid(&r);           /* 15 */
 strcpy(r.date,"2026-10-25");
 strcpy(r.impact,"Challenges");pass+=valid(&r);          /* 16 */
 strcpy(r.impact,"PotentialChange");pass+=valid(&r);    /* 17 */
 strcpy(r.impact,"Reinforces");pass+=valid(&r);          /* 18 */
 strcpy(r.kind,"STANDARD");r.milestone[0]=0;pass+=valid(&r); /* 19 */
 strcpy(r.kind,"MISSION");pass+=!valid(&r);              /* 20 */
 strcpy(r.milestone,"Published");
 r.summary[0]='\t';pass+=!valid(&r);                   /* 21 */
 strcpy(r.summary,"Reported demonstration");
 r.title[0]=0;pass+=!valid(&r);                        /* 22 */
 strcpy(r.title,"Relay demo");
 pass+=!valid(NULL);                                    /* 23 */
 pass+=!date_ok("2026-02-30");                          /* 24 */
 pass+=date_ok("2024-02-29");                           /* 25 */
 pass+=!date_ok("2025-02-29");                          /* 26 */
 q->tests_executed=26;q->tests_passed=pass;
 q->tests_failed=q->tests_executed-pass;
 q->negative_test_executed=1;q->negative_test_passed=!valid(NULL);
 return q->tests_failed||!q->negative_test_passed?
  STNLABZ_MODULE_ERR_QUALIFICATION:STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host){
 /* Digit Core startup_host supplies service registration, not command registration. */
 if(!host||!host->register_service||!host->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!host->register_service(DIGIT_ST1_SERVICE,query_service,NULL))
  return STNLABZ_MODULE_ERR_START_FAILED;
 module_host=host;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 int ok=1;
 if(module_host&&module_host->unregister_service)
  ok=module_host->unregister_service(DIGIT_ST1_SERVICE,NULL);
 module_host=NULL;
 return ok?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_STOP_FAILED;
}
static const stnlabz_module_descriptor_t descriptor={
 "st1","ST-1 Research Registry",0,1,0,
 STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
 qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){
 return &descriptor;
}
#endif
/* [End AI:GPT-6] */
