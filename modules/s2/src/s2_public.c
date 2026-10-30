#define _POSIX_C_SOURCE 200809L
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "s2_public.h"
#include "s2_research.h"


/* Rictus is a second record of the candidate, not independent proof.
 * Read only candidate IDs from a bounded tail of the local log. */
static size_t rictus_candidate_matches(struct json_object *rows, size_t n, int *available)
{
 FILE *file=fopen("/var/log/rictus/rictus.log","r");
 char line[1024],id[96];unsigned char *seen;size_t matched=0,i;
 long end;
 *available=0;
 if(!file)return 0;
 if(fseek(file,0,SEEK_END)!=0||(end=ftell(file))<0){fclose(file);return 0;}
 if(end>4194304L){
  if(fseek(file,end-4194304L,SEEK_SET)!=0){fclose(file);return 0;}
  if(!fgets(line,sizeof line,file)){fclose(file);return 0;}
 }else rewind(file);
 seen=calloc(n?n:1U,1);
 if(!seen){fclose(file);return 0;}
 *available=1;
 while(fgets(line,sizeof line,file)){
  const char *start,*stop;
  size_t idlen;
  if(!strstr(line," INTELLIGENCE CANDIDATE id="))continue;
  start=strstr(line," id=");
  if(!start)continue;
  start+=4;stop=strchr(start,' ');
  if(!stop)continue;
  idlen=(size_t)(stop-start);
  if(idlen<3U||idlen>=sizeof id||strncmp(start,"t_",2))continue;
  memcpy(id,start,idlen);id[idlen]=0;
  for(i=0;i<n;++i){
   struct json_object *row=json_object_array_get_idx(rows,i),*v=NULL;
   if(seen[i]||!row||!json_object_object_get_ex(row,"id",&v)||
      !json_object_is_type(v,json_type_string))continue;
   if(!strcmp(id,json_object_get_string(v))){seen[i]=1;++matched;break;}
  }
 }
 if(ferror(file))*available=0;
 free(seen);fclose(file);
 return *available?matched:0;
}

/* Public output is assembled exclusively from numeric aggregates and fixed,
 * reviewed prose. No arbitrary source strings, IPs, URLs, domains, internal
 * practices, request bodies or credentials can cross this boundary. */
int s2_public_render(const char *source_json, size_t source_size,
                     size_t tracked, size_t opened, size_t changed,
                     time_t collected_at, char *out, size_t capacity)
{
 struct json_object *root=NULL,*rows=NULL;
 struct json_tokener *tok=NULL;
 struct tm utc;
 size_t i,n,medium=0,high=0,critical=0,missing=0;
 size_t status_present=0,blocked=0,server_error=0;
 size_t cms_probes=0,config_probes=0,unclassified=0,rictus_matched=0;
 int rictus_available=0;
 char rictus_note[350];
 char when[40];
 char provenance[800];
 int written;
 if(!source_json||!source_size||source_size>8U*1024U*1024U||
    !out||capacity<1024U||!gmtime_r(&collected_at,&utc)) return 0;
 if(!strftime(when,sizeof when,"%Y-%m-%d %H:%M UTC",&utc)) return 0;
 tok=json_tokener_new();
 if(!tok)return 0;
 root=json_tokener_parse_ex(tok,source_json,(int)source_size);
 if(json_tokener_get_error(tok)!=json_tokener_success||!root||
    !json_object_is_type(root,json_type_object)||
    !json_object_object_get_ex(root,"threats",&rows)||
    !json_object_is_type(rows,json_type_array))goto fail;
 n=json_object_array_length(rows);
 if(n>10000U)goto fail;
 for(i=0;i<n;++i){
  struct json_object *item=json_object_array_get_idx(rows,i),*value=NULL,*details=NULL;
  const char *severity=NULL;
  if(!item||!json_object_is_type(item,json_type_object))goto fail;
  if(json_object_object_get_ex(item,"severity",&value)&&json_object_is_type(value,json_type_string))
    severity=json_object_get_string(value);
  if(json_object_object_get_ex(item,"details",&details)&&json_object_is_type(details,json_type_object)){
    if(!severity&&json_object_object_get_ex(details,"severity",&value)&&json_object_is_type(value,json_type_string))
      severity=json_object_get_string(value);
  }
  if(!severity)missing++;
  else if(!strcmp(severity,"MEDIUM"))medium++;
  else if(!strcmp(severity,"HIGH"))high++;
  else if(!strcmp(severity,"CRITICAL"))critical++;
  /* Case-level investigative triage: retain only category counts publicly.
   * A signature match means a review lead, never actor attribution. */
  {
   const char *path=NULL;
   struct json_object *candidate=NULL;
   if(json_object_object_get_ex(item,"request",&candidate) &&
      json_object_is_type(candidate,json_type_string))
     path=json_object_get_string(candidate);
   else if(json_object_object_get_ex(item,"path",&candidate) &&
           json_object_is_type(candidate,json_type_string))
     path=json_object_get_string(candidate);
   else if(details &&
           json_object_object_get_ex(details,"request_url",&candidate) &&
           json_object_is_type(candidate,json_type_string))
     path=json_object_get_string(candidate);
   else if(details &&
           json_object_object_get_ex(details,"request",&candidate) &&
           json_object_is_type(candidate,json_type_string))
     path=json_object_get_string(candidate);
   if(path&&(strstr(path,"/wp-admin/")||strstr(path,"/wp-login.php")))
     cms_probes++;
   else if(path&&(strstr(path,"/.env")||strstr(path,"/config.php")||
                  strstr(path,"/phpunit/")))
     config_probes++;
   else unclassified++;
  }
  value=NULL;
  if((json_object_object_get_ex(item,"response_status",&value)&&json_object_is_type(value,json_type_int))||
     (details&&json_object_object_get_ex(details,"response_status",&value)&&json_object_is_type(value,json_type_int))){
    int status=json_object_get_int(value);
    if(status>=100&&status<=599){
      status_present++;
      if(status==403||status==429)blocked++;
      if(status>=500)server_error++;
    }
  }
 }
 rictus_matched=rictus_candidate_matches(rows,n,&rictus_available);
 if(rictus_available)
  snprintf(rictus_note,sizeof rictus_note,
   "Rictus candidate log matched %zu of %zu current Sentinel record IDs. "
   "This confirms a matching candidate audit entry, not an independent observation, incident or compromise.",
   rictus_matched,n);
 else snprintf(rictus_note,sizeof rictus_note,
   "Rictus candidate log unavailable or unreadable to S2. No Rictus correlation is claimed.");
 if(!s2_httpbl_review(source_json,source_size,provenance,sizeof provenance))return 0;
 written=snprintf(out,capacity,
 "# STN-LABZ | SECURITY INTELLIGENCE\n\n"
 "## S2 DECISION BRIEF\n\n"
 "**Report Generated:** %s\n\n"
 "## BUSINESS DECISION\n\n"
 "**Triage:** {color:amber}MEDIUM = OBSERVE{/color}; {color:red}HIGH = ASSESS / CRITICAL = INVESTIGATE{/color}.\n\n"
 "Current Sentinel snapshot: %zu source records. MEDIUM: %zu. HIGH: %zu. CRITICAL: %zu. Severity classifications are not verified incidents.\n\n"
 "**Decision:** Continue observation for MEDIUM records. Review HIGH evidence for actual operational exposure. Investigate CRITICAL evidence. Initiate incident response only when independently supported by a material event or impact.\n\n"
 "## SUPPORTING EVIDENCE\n\n"
 "Case register: %zu; new records: %zu; revised records: %zu.\n\n"
 "Request patterns: %zu CMS login/admin paths; %zu configuration/test-file paths; %zu other or unclassified.\n\n"
 "HTTP statuses available: %zu of %zu. Recorded 403/429: %zu. Recorded 5xx: %zu.\n\n"
 "**Rictus record correlation:** %s\n\n"
 "Matching candidate IDs establishes an audit trail, not independent verification of an attack.\n\n"
 "## REMAINING INVESTIGATION\n\n"
 "Validate IP field roles, source times and response outcomes. Compare relevant HIGH and CRITICAL candidates with independent web, application, authentication and firewall logs before concluding an incident occurred. No breach or business disruption is established by this snapshot alone.\n\n"
 "## BUSINESS / OPERATIONAL EFFECTS\n\n"
 "Continue controls and evidence retention. If corroborating logs indicate unauthorized access, outage or customer impact, activate the documented incident procedure. If evidence supports only blocked administrative-path scanning, no emergency intervention is warranted.\n\n",
 when,n,medium,high,critical,tracked,opened,changed,cms_probes,config_probes,unclassified,status_present,n,blocked,server_error,rictus_note);
 json_object_put(root);json_tokener_free(tok);
 return written>0&&(size_t)written<capacity;
fail:
 if(root)json_object_put(root);
 json_tokener_free(tok);
 return 0;
}
