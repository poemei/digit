#define _POSIX_C_SOURCE 200809L
#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "s2_public.h"

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
 char when[40];
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
  value=NULL;
  if(json_object_object_get_ex(item,"response_status",&value)&&json_object_is_type(value,json_type_int)){
    int status=json_object_get_int(value);
    if(status>=100&&status<=599){
      status_present++;
      if(status==403||status==429)blocked++;
      if(status>=500)server_error++;
    }
  }
 }
 written=snprintf(out,capacity,
 "# STN-LABZ | SECURITY INTELLIGENCE\n\n"
 "## S2 INTELLIGENCE BRIEF\n\n"
 "**Report Generated:** %s\n\n---\n\n"
 "## 1. EXECUTIVE SUMMARY\n\n"
 "**Status:** {color:red}MEDIUM / HIGH / CRITICAL{/color} — Under Investigation\n\n"
 "Sentinel source observations are investigative leads, not confirmed compromises. "
 "Rictus corroboration was not performed in this collection.\n\n"
 "**Case Register:** %zu source-record cases.\n\n"
 "**Latest Poll:** %zu opened; %zu revised.\n\n---\n\n"
 "## 2. ACTIVE INVESTIGATIONS\n\n"
 "**Tracked Cases:** %zu source-record cases.\n\n"
 "S2 retained source record identifiers and change signatures. "
 "Investigators must corroborate any material revisions against source logs before classifying incidents. "
 "Historical bulk ingestion is not evidence of newly occurring attacks.\n\n---\n\n"
 "## 3. THREAT INTELLIGENCE OVERVIEW\n\n"
 "**Investigation Threshold:** {color:red}MEDIUM / HIGH / CRITICAL{/color}\n\n"
 "| Source severity | Records in current Sentinel snapshot |\n"
 "|---|---:|\n| MEDIUM | %zu |\n| HIGH | %zu |\n| CRITICAL | %zu |\n"
 "| Severity not supplied | %zu |\n\n"
 "These values count source records, not verified attacks or unique actors.\n\n---\n\n"
 "## 4. CROSS-SOURCE INTELLIGENCE\n\n"
 "Independent Rictus correlation is not available in this S2 revision. "
 "No cross-source match or attribution is claimed. Shared indicators alone cannot establish compromise.\n\n---\n\n"
 "## 5. INTELLIGENCE GAPS\n\n"
 "Rictus source availability and independent correlation, verified IP field roles, "
 "validated event chronology, attribution, exploitation outcomes, and confirmed incident impacts "
 "remain unestablished by this ingestion.\n\n---\n\n"
 "## 6. FINAL ASSESSMENT\n\n"
 "Prioritize MEDIUM, HIGH and CRITICAL records for evidence review. Verify source context, "
 "response outcomes and timelines before escalation. No breach or downtime is inferred from severity alone.\n\n---\n\n"
 "# INVESTIGATION FINDINGS\n\n"
 "## 7. RECENT FINDINGS\n\n"
 "**Reporting Window:** Not established; the feed does not provide a validated recent-time filter.\n\n"
 "Newly opened in latest ingestion: %zu. Revised: %zu. "
 "These counts are not assertions of newly occurring attacks.\n\n"
 "## 8. HISTORICAL FINDINGS\n\n"
 "Individual historical case identities and source-field values are retained privately. "
 "A public grouping by date or actor is withheld until field roles and chronology are verified.\n\n"
 "### SOURCE RESPONSE EVIDENCE\n\n"
 "Valid HTTP response status supplied: %zu. Observed 403/429: %zu. "
 "Observed HTTP 5xx: %zu. Missing/invalid status records: %zu.\n\n"
 "HTTP status observations alone do not establish downtime, exploitation or disclosure.\n\n"
 "## 9. REPORT CONCLUSION\n\n"
 "**Overall Assessment:** Active investigation; compromise unconfirmed.\n\n"
 "Preserve source evidence and corroborate before attribution.\n\n"
 "**STN-LABZ — S2 Security Intelligence**\n\n"
 "*Evidence before attribution. Investigation before conclusion.*\n\n---\n\n"
 "## BUSINESS / OPERATIONAL EFFECTS\n\n"
 "If validated as malicious and successful, the observed activity could lead to service disruption, "
 "increased investigative and remediation costs, unauthorized access, or business interruption. "
 "The present evidence does not establish successful exploitation, data disclosure, downtime, "
 "customer harm, or financial loss. Confirmation requires application and response logs, uptime "
 "monitoring, investigated incident records, and documented service-owner assessments.\n",
 when,tracked,opened,changed,tracked,medium,high,critical,missing,opened,changed,
 status_present,blocked,server_error,n-status_present);
 json_object_put(root);json_tokener_free(tok);
 return written>0&&(size_t)written<capacity;
fail:
 if(root)json_object_put(root);
 json_tokener_free(tok);
 return 0;
}
