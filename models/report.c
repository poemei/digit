#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "report.h"
#include "agent_root.h"
#include "digit_version.h"
static int has(const char *s,const char *term){
 char buf[600];size_t i,n;
 if(!s)return 0;
 n=strlen(s);if(n>=sizeof buf)return 0;
 for(i=0;i<n;i++)buf[i]=(char)tolower((unsigned char)s[i]);
 buf[n]=0;return strstr(buf,term)!=NULL;
}
digit_report_kind_t digit_report_classify(const char *s){
 if(has(s,"mission")||has(s,"purpose"))return DIGIT_REPORT_MISSION;
 if(has(s,"general order")||has(s,"orders"))return DIGIT_REPORT_ORDERS;
 if(has(s,"family")||has(s,"momma")||has(s,"auntie")||has(s,"sister")||has(s,"laurie")||has(s,"poe"))return DIGIT_REPORT_FAMILY;
 if(has(s,"core rule")||has(s,"core protection")||has(s,"modify your core"))return DIGIT_REPORT_RULE;
 if(has(s,"version"))return DIGIT_REPORT_VERSION;
 if(has(s,"who are you")||has(s,"identity")||has(s,"your name"))return DIGIT_REPORT_IDENTITY;
 return DIGIT_REPORT_UNKNOWN;
}
static size_t chunks(const char *s){
 size_t n=0,off=0,len;
 if(!s)return 0;
 len=strlen(s);
 while(off<len){size_t end=off+300;
  if(end>=len){n++;break;}
  while(end>off+100&&s[end]!=' ')end--;
  if(end<=off+100)end=off+300;
  off=end;while(s[off]==' ')off++;n++;
 }
 return n;
}
static int chunk(const char *s,size_t index,char *out,size_t cap){
 size_t off=0,len=strlen(s),i;
 for(i=0;i<=index&&off<len;i++){
  size_t end=off+300,n;
  if(end>=len)end=len;
  else {while(end>off+100&&s[end]!=' ')end--;if(end<=off+100)end=off+300;}
  if(i==index){n=end-off;if(n+1>cap)return 0;memcpy(out,s+off,n);out[n]=0;return 1;}
  off=end;while(s[off]==' ')off++;
 }
 return 0;
}
size_t digit_report_lines(digit_report_kind_t kind){
 const stnlabz_agent_root_t *r=stnlabz_agent_root_identity();
 switch(kind){case DIGIT_REPORT_ORDERS:case DIGIT_REPORT_FAMILY:return 3;
 case DIGIT_REPORT_MISSION:return chunks(r->mission);
 case DIGIT_REPORT_RULE:return chunks(r->core_protection_rule);
 case DIGIT_REPORT_IDENTITY:case DIGIT_REPORT_VERSION:return 1;
 default:return 0;}
}
int digit_report_line(digit_report_kind_t kind,size_t i,char *out,size_t cap){
 const stnlabz_agent_root_t *r=stnlabz_agent_root_identity();int n=-1;
 if(!out||!cap||!stnlabz_agent_root_validate(r)||i>=digit_report_lines(kind))return 0;
 switch(kind){
 case DIGIT_REPORT_MISSION:return chunk(r->mission,i,out,cap);
 case DIGIT_REPORT_ORDERS:n=snprintf(out,cap,"General Order %zu: %s",i+1,r->orders[i]);break;
 case DIGIT_REPORT_FAMILY:n=snprintf(out,cap,"%s is my %s (%s).",r->family[i].id,r->family[i].relationship,r->family[i].familiar_name);break;
 case DIGIT_REPORT_RULE:return chunk(r->core_protection_rule,i,out,cap);
 case DIGIT_REPORT_VERSION:n=snprintf(out,cap,"Digit %s",DIGIT_VERSION_STRING);break;
 case DIGIT_REPORT_IDENTITY:n=snprintf(out,cap,"I am %s, STN-LABZ %s.",r->name,r->lineage);break;
 default:return 0;
 }
 return n>=0&&(size_t)n<cap;
}
