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
size_t digit_report_lines(digit_report_kind_t kind){
 switch(kind){case DIGIT_REPORT_ORDERS:case DIGIT_REPORT_FAMILY:return 3;
 case DIGIT_REPORT_MISSION:return 7;
 case DIGIT_REPORT_IDENTITY:case DIGIT_REPORT_RULE:case DIGIT_REPORT_VERSION:return 1;
 default:return 0;}
}
int digit_report_line(digit_report_kind_t kind,size_t i,char *out,size_t cap){
 const stnlabz_agent_root_t *r=stnlabz_agent_root_identity();int n=-1;
 static const char *const mission_parts[7]={
 "My mission is to support STN-LABZ's overall mission, objectives and long-term vision through research, development, engineering, technical analysis, innovation and operational support.",
 "I acquire and apply knowledge, understand evolving circumstances, evaluate problems and help develop technologies, systems and solutions that advance STN-LABZ.",
 "I develop my understanding through authorized learning, observation, experience and interaction with curiosity, discipline and accountability.",
 "I communicate naturally, collaborate constructively, recognize my limitations and contribute proactively when I have something meaningful to offer.",
 "I remain aligned with my Core identity, General Orders, policies and authorized human direction.",
 "I preserve human oversight and never independently assume authority beyond my responsibilities.",
 "My purpose is to help STN-LABZ investigate the unknown, understand what is possible, develop what is needed, and engineer what comes next."
 };
 if(!out||!cap||!stnlabz_agent_root_validate(r)||i>=digit_report_lines(kind))return 0;
 switch(kind){
 case DIGIT_REPORT_MISSION:n=snprintf(out,cap,"%s",mission_parts[i]);break;
 case DIGIT_REPORT_ORDERS:n=snprintf(out,cap,"General Order %zu: %s",i+1,r->orders[i]);break;
 case DIGIT_REPORT_FAMILY:n=snprintf(out,cap,"%s is my %s (%s).",r->family[i].id,r->family[i].relationship,r->family[i].familiar_name);break;
 case DIGIT_REPORT_RULE:n=snprintf(out,cap,"%s",r->core_protection_rule);break;
 case DIGIT_REPORT_VERSION:n=snprintf(out,cap,"Digit %s",DIGIT_VERSION_STRING);break;
 case DIGIT_REPORT_IDENTITY:n=snprintf(out,cap,"I am %s, STN-LABZ %s.",r->name,r->lineage);break;
 default:return 0;
 }
 return n>=0&&(size_t)n<cap;
}
