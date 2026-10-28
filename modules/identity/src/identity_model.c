#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "identity_model.h"

static int bounded_copy(char *out,size_t cap,const char *s)
{
 size_t n;
 if (!out || !s) return 0;
 n=strlen(s);
 if (!n || n>=cap) return 0;
 memcpy(out,s,n+1U);
 return 1;
}
static int set_field(identity_record_t *r,const char *key,const char *value,unsigned *mask)
{
 unsigned bit=0;
 int ok=0;
 if (!strcmp(key,"name")) {bit=1U;ok=bounded_copy(r->name,sizeof r->name,value);}
 else if (!strcmp(key,"organization")) {bit=2U;ok=bounded_copy(r->organization,sizeof r->organization,value);}
 else if (!strcmp(key,"role")) {bit=4U;ok=bounded_copy(r->role,sizeof r->role,value);}
 else if (!strcmp(key,"mission")) {bit=8U;ok=bounded_copy(r->mission,sizeof r->mission,value);}
 else return 0;
 if (!ok || (*mask & bit)) return 0;
 *mask|=bit;
 return 1;
}
int identity_load(const char *path,identity_record_t *out)
{
 FILE *fp;
 char line[1536];
 identity_record_t result={0};
 unsigned mask=0U;
 if (!path || !out) return 0;
 fp=fopen(path,"r");
 if (!fp) return 0;
 while (fgets(line,sizeof line,fp)) {
  char *eq,*end,*key=line;
  size_t len=strlen(line);
  if (!len || (line[len-1]!='\n' && !feof(fp))) goto fail;
  end=line+len;
  while (end>line && (end[-1]=='\n'||end[-1]=='\r')) *--end=0;
  while (*key==' '||*key=='\t') ++key;
  if (!*key || *key=='#') continue;
  eq=strchr(key,'=');
  if (!eq) goto fail;
  *eq++=0;
  if (!set_field(&result,key,eq,&mask)) goto fail;
 }
 if (ferror(fp) || fclose(fp)!=0) return 0;
 if (mask!=15U) return 0;
 *out=result;
 return 1;
fail:
 (void)fclose(fp);
 return 0;
}
static int equals_ci(const char *a,const char *b)
{
 for (;*a&&*b;++a,++b)
  if (tolower((unsigned char)*a)!=tolower((unsigned char)*b)) return 0;
 return !*a&&! *b;
}
int identity_reply(const identity_record_t *r,const char *query,char *out,size_t cap)
{
 const char *value=NULL;
 int n;
 if (!r||!query||!out||cap<2U) return 0;
 out[0]=0;
 if (!*query||equals_ci(query,"mission")) value=r->mission;
 else if (equals_ci(query,"who")||equals_ci(query,"identity")) {
  n=snprintf(out,cap,"I am %s, a %s serving %s.",r->name,r->role,r->organization);
  return n>0 && (size_t)n<cap;
 } else if (equals_ci(query,"orders")||equals_ci(query,"general orders")||
            equals_ci(query,"first general order")||equals_ci(query,"second general order")||
            equals_ci(query,"third general order")) {
  value="My General Orders are protected by Core. This module has no authorized Core read service for them; I will not invent an answer.";
 } else value="Identity commands: /identity mission, /identity who, /identity orders. General Orders require an authorized Core read interface.";
 return bounded_copy(out,cap,value);
}
static void check(int ok,unsigned *e,unsigned *p){++*e;if(ok)++*p;}
int identity_selftest(unsigned *executed,unsigned *passed,int *negative_executed,int *negative_passed)
{
 identity_record_t r={0};
 char b[1200];
 unsigned e=0,p=0,mask=0;
 int ne,np;
 check(set_field(&r,"name","Digit",&mask),&e,&p);
 check(set_field(&r,"organization","STN-LABZ",&mask),&e,&p);
 check(set_field(&r,"role","terrestrial AI agent",&mask),&e,&p);
 check(set_field(&r,"mission","Approved mission",&mask),&e,&p);
 check(mask==15U,&e,&p);
 check(identity_reply(&r,"mission",b,sizeof b)&&!strcmp(b,"Approved mission"),&e,&p);
 check(identity_reply(&r,"who",b,sizeof b)&&strstr(b,"Digit")!=NULL,&e,&p);
 check(identity_reply(&r,"MISSION",b,sizeof b)&&!strcmp(b,"Approved mission"),&e,&p);
 check(identity_reply(&r,"orders",b,sizeof b)&&strstr(b,"will not invent")!=NULL,&e,&p);
 check(identity_reply(&r,"unknown",b,sizeof b)&&strstr(b,"Identity commands")!=NULL,&e,&p);
 ne=1;np=!set_field(&r,"mission","unauthorized duplicate",&mask);
 if(executed)*executed=e;
 if(passed)*passed=p;
 if(negative_executed)*negative_executed=ne;
 if(negative_passed)*negative_passed=np;
 return e==10U&&p==10U&&ne&&np;
}
