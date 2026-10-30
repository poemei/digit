#define _POSIX_C_SOURCE 200809L
#include <json-c/json.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "s2_public.h"

#define S2_RICTUS_PATH "/var/log/rictus/rictus.log"
#define S2_RICTUS_LIMIT 4194304L
#define S2_MAX_CASES 10000U
typedef struct {
 char id[96];
 char observed[20];
 char category[48];
 int severity;
 int status;
 int rictus;
 int chronology;
} finding_t;
static const char *string_field(struct json_object *o,const char *key)
{
 struct json_object *v=NULL;
 return o&&json_object_object_get_ex(o,key,&v)&&
   json_object_is_type(v,json_type_string)?json_object_get_string(v):NULL;
}
static struct json_object *details_of(struct json_object *o)
{
 struct json_object *d=NULL;
 return o&&json_object_object_get_ex(o,"details",&d)&&
   json_object_is_type(d,json_type_object)?d:NULL;
}
static const char *field_both(struct json_object *o,struct json_object *d,const char *key)
{
 const char *v=string_field(o,key);
 return v?v:string_field(d,key);
}
static int status_both(struct json_object *o,struct json_object *d)
{
 struct json_object *v=NULL;
 if(!json_object_object_get_ex(o,"response_status",&v)||
    !json_object_is_type(v,json_type_int)){
  if(!d||!json_object_object_get_ex(d,"response_status",&v)||
     !json_object_is_type(v,json_type_int))return 0;
 }
 {int code=json_object_get_int(v);return code>=100&&code<=599?code:0;}
}
static int sev(const char *s)
{
 if(!s)return 0;
 if(!strcmp(s,"MEDIUM"))return 1;
 if(!strcmp(s,"HIGH"))return 2;
 if(!strcmp(s,"CRITICAL"))return 3;
 return 0;
}
static const char *classify(const char *path)
{
 if(!path)return "path unavailable";
 if(strstr(path,"/wp-admin")||strstr(path,"/wp-login.php"))
  return "CMS administration/login path";
 if(strstr(path,"/.env")||strstr(path,"/config.php")||
    strstr(path,"/phpunit")||strstr(path,"/.git/"))
  return "configuration or test-file path";
 return "other request path";
}
static int add(char *out,size_t cap,size_t *used,const char *format,...)
{
 va_list ap;int n;
 if(*used>=cap)return 0;
 va_start(ap,format);n=vsnprintf(out+*used,cap-*used,format,ap);va_end(ap);
 if(n<0||(size_t)n>=cap-*used)return 0;
 *used+=(size_t)n;return 1;
}
/* Log is an audit trail of candidate IDs, not an independent web-log witness.
 * Every match must be tied to an exact Sentinel ID. */
static size_t correlate(finding_t *cases,size_t count,int *available,size_t *timed)
{
 FILE *fp=fopen(S2_RICTUS_PATH,"r");
 char line[1024];
 long length;
 size_t i,matched=0;
 *available=0;*timed=0;
 if(!fp)return 0;
 if(fseek(fp,0,SEEK_END)!=0||(length=ftell(fp))<0){fclose(fp);return 0;}
 if(length>S2_RICTUS_LIMIT){
  if(fseek(fp,length-S2_RICTUS_LIMIT,SEEK_SET)!=0){fclose(fp);return 0;}
  if(!fgets(line,sizeof line,fp)){fclose(fp);return 0;}
 }else rewind(fp);
 *available=1;
 while(fgets(line,sizeof line,fp)){
  char id[96],observed[20];
  const char *p,*e,*o;
  size_t len;
  if(!strstr(line," INTELLIGENCE CANDIDATE id="))continue;
  p=strstr(line," id=");if(!p)continue;
  p+=4;e=strchr(p,' ');if(!e)continue;
  len=(size_t)(e-p);
  if(len<3||len>=sizeof id||strncmp(p,"t_",2))continue;
  memcpy(id,p,len);id[len]=0;
  observed[0]=0;
  o=strstr(e," observed=");
  if(o){
   o+=10;
   if(strlen(o)>=19){memcpy(observed,o,19);observed[19]=0;}
  }
  for(i=0;i<count;++i){
   if(strcmp(cases[i].id,id))continue;
   if(!cases[i].rictus){cases[i].rictus=1;++matched;}
   if(observed[0]&&cases[i].observed[0]&&
      !strncmp(cases[i].observed,observed,19)&&!cases[i].chronology){
    cases[i].chronology=1;++*timed;
   }
   break;
  }
 }
 if(ferror(fp)){*available=0;matched=0;*timed=0;}
 fclose(fp);return matched;
}
int s2_public_render(const char *source_json,size_t source_size,
                     size_t tracked,size_t opened,size_t changed,
                     time_t collected_at,char *out,size_t capacity)
{
 struct json_object *root=NULL,*rows=NULL;
 struct json_tokener *tok=NULL;
 finding_t *cases=NULL;
 struct tm utc;
 char when[40];
 size_t i,n,used=0,medium=0,high=0,critical=0,unknown=0;
 size_t admin=0,config=0,other=0,blocked=0,server_errors=0,status_known=0;
 size_t linked=0,time_linked=0,red_linked=0,red_blocked=0,red_unknown=0,red_errors=0;
 size_t shown=0;int log_access=0,ok=0;
 if(!source_json||!source_size||source_size>8U*1024U*1024U||
    !out||capacity<1024||!gmtime_r(&collected_at,&utc))return 0;
 if(!strftime(when,sizeof when,"%Y-%m-%d %H:%M UTC",&utc))return 0;
 tok=json_tokener_new();if(!tok)return 0;
 root=json_tokener_parse_ex(tok,source_json,(int)source_size);
 if(json_tokener_get_error(tok)!=json_tokener_success||!root||
    !json_object_is_type(root,json_type_object)||
    !json_object_object_get_ex(root,"threats",&rows)||
    !json_object_is_type(rows,json_type_array))goto done;
 n=json_object_array_length(rows);
 if(n>S2_MAX_CASES)goto done;
 cases=calloc(n?n:1,sizeof *cases);if(!cases)goto done;
 for(i=0;i<n;++i){
  struct json_object *item=json_object_array_get_idx(rows,i),*d;
  const char *id,*severity,*path,*stamp;
  int status;
  if(!item||!json_object_is_type(item,json_type_object))goto done;
  d=details_of(item);id=string_field(item,"id");
  if(!id||strlen(id)>=sizeof cases[i].id||strncmp(id,"t_",2))goto done;
  snprintf(cases[i].id,sizeof cases[i].id,"%s",id);
  severity=field_both(item,d,"severity");
  cases[i].severity=sev(severity);
  if(cases[i].severity==1)++medium;
  else if(cases[i].severity==2)++high;
  else if(cases[i].severity==3)++critical;
  else ++unknown;
  path=field_both(item,d,"request_url");
  if(!path)path=field_both(item,d,"request");
  if(!path)path=field_both(item,d,"path");
  snprintf(cases[i].category,sizeof cases[i].category,"%s",classify(path));
  if(!strcmp(cases[i].category,"CMS administration/login path"))++admin;
  else if(!strcmp(cases[i].category,"configuration or test-file path"))++config;
  else ++other;
  stamp=string_field(item,"created_at");
  if(stamp&&strlen(stamp)>=19){
   memcpy(cases[i].observed,stamp,19);
   cases[i].observed[19]=0;
   if(cases[i].observed[10]=='T')cases[i].observed[10]=' ';
  }
  status=status_both(item,d);
  cases[i].status=status;
  if(status){
   ++status_known;
   if(status==403||status==429)++blocked;
   if(status>=500)++server_errors;
  }
  if(cases[i].severity>=2){
   if(status==403||status==429)++red_blocked;
   if(!status)++red_unknown;
   if(status>=500)++red_errors;
  }
 }
 linked=correlate(cases,n,&log_access,&time_linked);
 for(i=0;i<n;++i)if(cases[i].severity>=2&&cases[i].rictus)++red_linked;
 if(!add(out,capacity,&used,
  "# STN-LABZ | SECURITY INTELLIGENCE\n\n"
  "## S2 EVIDENCE-BASED BUSINESS ASSESSMENT\n\n"
  "**Generated:** %s\n\n"
  "## DECISION\n\n"
  "**Current disposition:** {color:amber}MEDIUM — OBSERVE{/color}; "
  "{color:red}HIGH — ASSESS / CRITICAL — INVESTIGATE{/color}.\n\n"
  "In the current Sentinel snapshot, %zu records are MEDIUM, %zu HIGH, "
  "and %zu CRITICAL. These are source classifications, not confirmed incidents.\n\n",
  when,medium,high,critical))goto done;
 if(critical){
  if(!add(out,capacity,&used,
   "**Business decision:** CRITICAL-classified evidence requires immediate case investigation and independent response-log validation. "
   "Do not announce compromise without evidence of an actual outcome.\n\n"))goto done;
 }else if(high){
  if(!add(out,capacity,&used,
   "**Business decision:** Examine the %zu HIGH-classified cases before undertaking disruptive response. "
   "%zu of those cases have a recorded denial/rate-limit response, while %zu have no valid response status. "
   "These figures do not establish successful access or complete protection.\n\n",
   high,red_blocked,red_unknown))goto done;
 }else{
  if(!add(out,capacity,&used,
   "**Business decision:** Continue routine monitoring. No RED-classified evidence appears in this snapshot.\n\n"))goto done;
 }
 if(!add(out,capacity,&used,
  "## WHAT DIGIT VERIFIED FROM AVAILABLE RECORDS\n\n"
  "- Sentinel source records: %zu; register entries: %zu; new/revised at latest collection: %zu/%zu.\n"
  "- Request paths: %zu CMS admin/login; %zu configuration/test-file; %zu other/unclassified.\n"
  "- Response codes: %zu provided; %zu reported 403/429; %zu reported HTTP 5xx.\n",
  n,tracked,opened,changed,admin,config,other,status_known,blocked,server_errors))goto done;
 if(log_access){
  if(!add(out,capacity,&used,
   "- Rictus candidate log: %zu exact record-ID matches; %zu also agree on the recorded event time; "
   "%zu HIGH/CRITICAL records have a Rictus candidate entry.\n\n",
   linked,time_linked,red_linked))goto done;
 }else{
  if(!add(out,capacity,&used,
   "- Rictus candidate log: unavailable to S2; no correlation claimed.\n\n"))goto done;
 }
 if(!add(out,capacity,&used,
  "A Rictus candidate entry provides an audit link, not an independent witness of an HTTP request. "
  "Neither a severity label nor a 403 response establishes a compromise or definitive protection.\n\n"
  "## HIGH / CRITICAL CASE ASSESSMENTS\n\n"))goto done;
 for(i=0;i<n&&shown<6;++i){
  const finding_t *c=&cases[i];
  if(c->severity<2)continue;
  ++shown;
  if(!add(out,capacity,&used,
   "**Case %zu — %s:** %s; recorded response %s",
   shown,c->severity==3?"CRITICAL":"HIGH",c->category,
   c->status?"available":"unavailable"))goto done;
  if(c->status){
   if(!add(out,capacity,&used," (%d)",c->status))goto done;
  }
  if(!add(out,capacity,&used,
   "; Rictus candidate ID %s%s. ",
   c->rictus?"matched":"not matched",
   c->chronology?" with matching observed time":""))goto done;
  if(c->status==403||c->status==429){
   if(!add(out,capacity,&used,
    "Reported denial/rate-limiting supports a blocked-request assessment, "
    "subject to independent web-log verification.\n\n"))goto done;
  }else if(c->status>=200&&c->status<400){
   if(!add(out,capacity,&used,
    "A successful HTTP status alone cannot establish unauthorized access; "
    "review authentication and application logs.\n\n"))goto done;
  }else{
   if(!add(out,capacity,&used,
    "Access outcome remains unresolved; examine the corresponding server response and authentication log.\n\n"))goto done;
  }
 }
 if(!shown){
  if(!add(out,capacity,&used,
   "No HIGH or CRITICAL cases were classified in this snapshot.\n\n"))goto done;
 }
 if(!add(out,capacity,&used,
  "## BUSINESS / OPERATIONAL EFFECTS\n\n"
  "The evidence above supports targeted verification of RED-classified records, "
  "not an assertion of successful compromise. Assess customer or service impact only "
  "when independently recorded access outcomes, authentication events, and service-health "
  "evidence justify it. Preserve case IDs privately for audit and incident follow-up.\n"))goto done;
 ok=1;
done:
 free(cases);
 if(root)json_object_put(root);
 if(tok)json_tokener_free(tok);
 return ok;
}
