#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <json-c/json.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "s2_investigation.h"

/* AI-assisted: GPT-6, 2026-11-02 UTC.
 * Collectors and reporters must not convert repeated source records into
 * claims of new attacks. Investigation journal times are COLLECTION times. */
#define MAX_ENTRIES 10000U
#define MAX_SOURCE (8U * 1024U * 1024U)
#define JOURNAL_MAGIC "S2INV1"
typedef struct {
    char id[96];
    uint64_t hash;
    int64_t first, last;
    unsigned cycles, revisions, escalations;
    int severity;
} investigation_t;
typedef struct {
    investigation_t *rows;
    size_t count;
} journal_t;

static const char *severity_name(int level)
{
    return level == 3 ? "CRITICAL" : level == 2 ? "HIGH" :
           level == 1 ? "MEDIUM" : "UNCLASSIFIED";
}
static int severity_of(struct json_object *row)
{
    struct json_object *v = NULL, *detail = NULL;
    const char *value;
    if (!json_object_object_get_ex(row, "severity", &v) ||
        !json_object_is_type(v, json_type_string)) {
        if (!json_object_object_get_ex(row, "details", &detail) ||
            !json_object_is_type(detail, json_type_object) ||
            !json_object_object_get_ex(detail, "severity", &v) ||
            !json_object_is_type(v, json_type_string)) return 0;
    }
    value = json_object_get_string(v);
    return !strcmp(value,"CRITICAL") ? 3 : !strcmp(value,"HIGH") ? 2 :
           !strcmp(value,"MEDIUM") ? 1 : 0;
}
static const char *case_id(struct json_object *row)
{
    struct json_object *v = NULL;
    if (!row || !json_object_is_type(row,json_type_object) ||
        !json_object_object_get_ex(row,"id",&v) ||
        !json_object_is_type(v,json_type_string)) return NULL;
    return json_object_get_string(v);
}
static int valid_id(const char *id)
{
    size_t i;
    if (!id || id[0]!='t' || id[1]!='_' || !id[2]) return 0;
    for (i=2;id[i];++i)
        if (i>=95 || !((id[i]>='a'&&id[i]<='z') ||
                       (id[i]>='A'&&id[i]<='Z') ||
                       (id[i]>='0'&&id[i]<='9') ||
                       id[i]=='_' || id[i]=='.' || id[i]=='-')) return 0;
    return 1;
}
static investigation_t *find_investigation(journal_t *j,const char *id)
{
    size_t i;
    for(i=0;i<j->count;++i)
        if(!strcmp(j->rows[i].id,id))return j->rows+i;
    return NULL;
}
static const s2_case_t *find_source(const s2_register_t *s,const char *id)
{
    size_t i;
    for(i=0;i<s->count;++i)
        if(!strcmp(s->cases[i].id,id))return &s->cases[i];
    return NULL;
}
static int load_journal(journal_t *j,const char *path)
{
    FILE *f;
    char line[320],magic[32];
    size_t n=0;
    if(!j || !path)return 0;
    f=fopen(path,"r");
    if(!f)return errno==ENOENT;
    if(!fgets(magic,sizeof magic,f)||strcmp(magic,JOURNAL_MAGIC "\n")){
        fclose(f);return 0;
    }
    while(fgets(line,sizeof line,f)){
        investigation_t record={0};
        char extra;
        int consumed;
        if(n>=MAX_ENTRIES || !strchr(line,'\n')){fclose(f);return 0;}
        consumed=sscanf(line,"%95s %" SCNx64 " %" SCNd64 " %" SCNd64
                        " %u %u %u %d %c",
                        record.id,&record.hash,&record.first,&record.last,
                        &record.cycles,&record.revisions,&record.escalations,
                        &record.severity,&extra);
        if(consumed!=8 || !valid_id(record.id) ||
           record.cycles<1U || record.first<=0 || record.last<record.first ||
           record.severity<0 || record.severity>3 ||
           find_investigation(j,record.id)){
            fclose(f);return 0;
        }
        j->rows[n++]=record;
        j->count=n;
    }
    if(ferror(f)){fclose(f);return 0;}
    return fclose(f)==0;
}
static int save_journal(const journal_t *j,const char *path)
{
    char temporary[1024];
    FILE *f;
    int fd,failed=0;
    size_t i;
    if(snprintf(temporary,sizeof temporary,"%s.tmp.XXXXXX",path)>=(int)sizeof temporary)
        return 0;
    fd=mkstemp(temporary);
    if(fd<0)return 0;
    if(fchmod(fd,0600)){close(fd);unlink(temporary);return 0;}
    f=fdopen(fd,"w");
    if(!f){close(fd);unlink(temporary);return 0;}
    if(fprintf(f,JOURNAL_MAGIC "\n")<0)failed=1;
    for(i=0;i<j->count && !failed;++i){
        const investigation_t *r=&j->rows[i];
        if(fprintf(f,"%s %016" PRIx64 " %" PRId64 " %" PRId64
                   " %u %u %u %d\n",r->id,r->hash,r->first,r->last,
                   r->cycles,r->revisions,r->escalations,r->severity)<0)failed=1;
    }
    if(fflush(f)!=0)failed=1;
    if(fsync(fd)!=0)failed=1;
    if(fclose(f)!=0)failed=1;
    if(failed){unlink(temporary);return 0;}
    if(rename(temporary,path)){unlink(temporary);return 0;}
    return 1;
}
static const char *field(struct json_object *row,const char *name)
{
    struct json_object *v=NULL,*detail=NULL;
    if(json_object_object_get_ex(row,name,&v) &&
       json_object_is_type(v,json_type_string))return json_object_get_string(v);
    if(json_object_object_get_ex(row,"details",&detail) &&
       json_object_is_type(detail,json_type_object) &&
       json_object_object_get_ex(detail,name,&v) &&
       json_object_is_type(v,json_type_string))return json_object_get_string(v);
    return NULL;
}
static const char *source_ip(struct json_object *row)
{
    const char *ip=field(row,"ip");
    return ip && *ip ? ip : field(row,"ip_address");
}
static int valid_address(const char *text)
{
    struct in_addr ipv4;
    struct in6_addr ipv6;
    return text && *text && strlen(text)<64U &&
           (inet_pton(AF_INET,text,&ipv4)==1 ||
            inet_pton(AF_INET6,text,&ipv6)==1);
}
static int status_of(struct json_object *row)
{
    struct json_object *v=NULL,*details=NULL;
    if(!json_object_object_get_ex(row,"response_status",&v) ||
       !json_object_is_type(v,json_type_int)){
        if(!json_object_object_get_ex(row,"details",&details) ||
           !json_object_is_type(details,json_type_object) ||
           !json_object_object_get_ex(details,"response_status",&v) ||
           !json_object_is_type(v,json_type_int))return 0;
    }
    { int status=json_object_get_int(v);return status>=100&&status<=599?status:0; }
}
static const char *request_category(struct json_object *row)
{
    const char *path=field(row,"request_url");
    if(!path)path=field(row,"request");
    if(!path)path=field(row,"path");
    if(!path||!*path)return "request path not supplied";
    if(strstr(path,"/wp-admin")||strstr(path,"/wp-login.php"))
        return "CMS login or administration probing";
    if(strstr(path,"/.env")||strstr(path,"/config.php")||
       strstr(path,"/.git/")||strstr(path,"/phpunit"))
        return "configuration or development-file probing";
    if(strstr(path,"/admin")||strstr(path,"/login"))
        return "application login or administration request";
    return "other or unclassified request";
}
static const char *response_finding(int status)
{
    if(status==401)return "HTTP 401 indicates an authentication challenge; access outcome unverified";
    if(status==403)return "HTTP 403 indicates the source reports a denied request";
    if(status==429)return "HTTP 429 indicates the source reports rate limiting";
    if(status>=200 && status<300)
        return "HTTP 2xx reports request-level success, not authenticated access or compromise";
    if(status>=300 && status<400)return "HTTP 3xx reports a redirect; target access is unverified";
    if(status>=500)return "HTTP 5xx reports a server error; operational impact is unverified";
    if(status>=400)return "HTTP 4xx reports request rejection; cause is not established";
    return "no valid HTTP response status provided; access outcome unknown";
}
static const char *next_check(int status,const char *category)
{
    if(status==403||status==429)
        return "Check local access and enforcement logs for the reported denial and related attempts";
    if(status>=200&&status<400)
        return "Check authentication and application logs before interpreting the response as access";
    if(status>=500)
        return "Check application error and availability logs before alleging service impact";
    if(strstr(category,"configuration")||strstr(category,"administration"))
        return "Check exact request handling against the protected resource and access logs";
    return "Obtain the original request and independently verified response before closing this lead";
}
static int source_day(const char *date)
{
    size_t i;
    if(!date || strlen(date)<10U || date[4]!='-' || date[7]!='-')return 0;
    for(i=0;i<10;++i)
        if(i!=4&&i!=7&&(date[i]<'0'||date[i]>'9'))return 0;
    return 1;
}
static int append(char *out,size_t cap,size_t *used,const char *format,...)
{
    va_list args;
    int n;
    if(*used>=cap)return 0;
    va_start(args,format);
    n=vsnprintf(out+*used,cap-*used,format,args);
    va_end(args);
    if(n<0 || (size_t)n>=cap-*used)return 0;
    *used+=(size_t)n;
    return 1;
}
static void format_utc(time_t when,char out[32])
{
    struct tm tm;
    if(!gmtime_r(&when,&tm) ||
       !strftime(out,32,"%Y-%m-%d %H:%M UTC",&tm))
        snprintf(out,32,"unavailable");
}
int s2_investigation_update(const char *source_json,size_t source_size,
                            const s2_register_t *state,const char *journal_path,
                            time_t collected_at,char *out,size_t cap)
{
    journal_t j={0};
    struct json_tokener *tok=NULL;
    struct json_object *root=NULL,*rows=NULL;
    size_t i,n,used=0,continuing=0,changed=0,escalated=0,baseline=0;
    size_t seen_high=0,seen_critical=0,unclassified=0;
    size_t examined=0,detail=0;
    int ok=0;
    int64_t epoch=(int64_t)collected_at;
    char collected[32];

    if(!source_json||!source_size||source_size>MAX_SOURCE||!state||!journal_path||
       !out||cap<1500||collected_at<=0 || (!state->cases && state->count))
        return 0;
    out[0]=0;
    j.rows=calloc(MAX_ENTRIES,sizeof *j.rows);
    if(!j.rows)return 0;
    if(!load_journal(&j,journal_path))goto finish;
    tok=json_tokener_new();
    if(!tok)goto finish;
    root=json_tokener_parse_ex(tok,source_json,(int)source_size);
    if(json_tokener_get_error(tok)!=json_tokener_success || !root ||
       !json_object_is_type(root,json_type_object) ||
       !json_object_object_get_ex(root,"threats",&rows) ||
       !json_object_is_type(rows,json_type_array))goto finish;
    n=json_object_array_length(rows);
    if(n>MAX_ENTRIES)goto finish;
    /* Validate the entire snapshot before writing any journal state. */
    for(i=0;i<n;++i){
        struct json_object *row=json_object_array_get_idx(rows,i);
        const char *id=case_id(row);
        size_t k;
        if(!valid_id(id)||!find_source(state,id))goto finish;
        for(k=0;k<i;++k)
            if(!strcmp(id,case_id(json_object_array_get_idx(rows,k))))goto finish;
    }
    for(i=0;i<n;++i){
        struct json_object *row=json_object_array_get_idx(rows,i);
        const char *id=case_id(row);
        const s2_case_t *source=find_source(state,id);
        investigation_t *record=find_investigation(&j,id);
        int sev=severity_of(row);
        if(!record){
            if(j.count>=MAX_ENTRIES)goto finish;
            record=&j.rows[j.count++];
            memset(record,0,sizeof *record);
            snprintf(record->id,sizeof record->id,"%s",id);
            record->first=epoch;
            record->cycles=1;
            ++baseline;
        }else{
            /* A clock moving backwards cannot rewrite established history. */
            if(record->last>epoch)goto finish;
            if(record->last!=epoch && record->cycles<UINT32_MAX){
                ++record->cycles;
                ++continuing;
            }
            if(record->hash!=source->hash && record->hash!=0 && record->revisions<UINT32_MAX){
                ++record->revisions;
                ++changed;
            }
            if(record->severity && sev>record->severity && record->escalations<UINT32_MAX){
                ++record->escalations;
                ++escalated;
            }
        }
        record->hash=source->hash;
        record->last=epoch;
        record->severity=sev;
        if(sev==2)++seen_high;
        else if(sev==3)++seen_critical;
        else if(!sev)++unclassified;
    }
    format_utc(collected_at,collected);
    if(!append(out,cap,&used,
       "\n\n## INVESTIGATION CONTINUITY\n\n"
       "**Journal updated:** %s. This section tracks Sentinel source-record continuity "
       "across S2 collection cycles, not independent attack events.\n\n"
       "- Source records examined in this collection: %zu; first recorded by this journal: %zu; "
       "previously recorded and observed again: %zu.\n"
       "- Revised retained source records: %zu; increased severity classifications: %zu.\n"
       "- Current HIGH records: %zu; CRITICAL records: %zu; without recognized severity: %zu.\n"
       "- Cumulative records in the investigation journal: %zu. "
       "Absence from a later snapshot is not case closure.\n\n",
       collected,n,baseline,continuing,changed,escalated,
       seen_high,seen_critical,unclassified,j.count))goto finish;

    if(!append(out,cap,&used,
       "### Priority investigative leads\n\n"
       "The following are source-record histories selected for review. "
       "Collection-cycle repetition is not proof of renewed hostile activity.\n\n"))goto finish;

    /* Rank by observed source changes, not invented attacker identity.
     * Evidence and response are taken from the individual source record.
     * Never emit raw case IDs, IPs, domains, URLs or arbitrary source text. */
    for(int pass=0;pass<4 && detail<12;++pass){
        for(i=0;i<n && detail<12;++i){
            struct json_object *row=json_object_array_get_idx(rows,i);
            const char *id=case_id(row),*category,*ip,*observed;
            investigation_t *r=find_investigation(&j,id);
            int priority,status;
            size_t related=0,related_high=0,k;
            char first[32],last[32],observed_text[24]="not supplied";
            if(!r || r->severity<2)continue;
            priority=r->escalations?0:r->revisions?1:r->cycles>1?2:3;
            if(priority!=pass)continue;
            category=request_category(row);
            status=status_of(row);
            observed=field(row,"created_at");
            if(source_day(observed)){
                size_t length=strlen(observed);
                if(length>=19 && (observed[10]==' '||observed[10]=='T')){
                    memcpy(observed_text,observed,19);
                    observed_text[19]=0;
                }else{
                    memcpy(observed_text,observed,10);
                    observed_text[10]=0;
                }
            }
            ip=source_ip(row);
            if(valid_address(ip)){
                for(k=0;k<n;++k){
                    struct json_object *other=json_object_array_get_idx(rows,k);
                    const char *other_ip=source_ip(other);
                    if(k==i||!valid_address(other_ip)||strcmp(ip,other_ip))continue;
                    ++related;
                    if(severity_of(other)>=2)++related_high;
                }
            }
            format_utc((time_t)r->first,first);
            format_utc((time_t)r->last,last);
            ++detail;
            if(!append(out,cap,&used,
                 "**Lead %zu — %s: %s**\\n\\n"
                 "- **Source observation:** %s (source timezone not independently verified).\\n"
                 "- **Observed behavior:** %s.\\n"
                 "- **Recorded response:** %s.\\n"
                 "- **Associated records:** %zu other source record(s) share an exact valid address in this snapshot; %zu classified HIGH/CRITICAL. "
                 "This may reflect a shared proxy or retained source events, not one actor.\\n"
                 "- **History:** First S2 collection %s; latest %s; %u collection cycle(s), %u source revision(s), %u severity escalation(s).\\n"
                 "- **Finding:** %s; %s.\\n"
                 "- **Investigation requirement:** %s.\\n\\n",
                 detail,severity_name(r->severity),category,observed_text,
                 category,response_finding(status),related,related_high,
                 first,last,r->cycles,r->revisions,r->escalations,
                 r->escalations?"The source's severity classification increased":
                 r->revisions?"The retained source record changed":
                 r->cycles>1?"This source record remains present in later S2 collections":
                 "This is the first journal observation of the source record",
                 status==403||status==429?"reported protective response; successful intrusion not established":
                 "independent incident outcome not established",
                 next_check(status,category)))goto finish;
        }
    }
    if(!detail && !append(out,cap,&used,
          "No recurring, revised, or escalating HIGH/CRITICAL source records qualified for priority review.\n"))
        goto finish;
    if(!append(out,cap,&used,
       "\n### Shared network indicators in the current snapshot\n\n"
       "This comparison joins exact recorded address values within the current "
       "source snapshot. It does not establish a common attacker or successful access.\n\n"))
       goto finish;
    {
        unsigned char *visited=calloc(n?n:1U,1U);
        size_t groups=0;
        if(!visited)goto finish;
        for(i=0;i<n&&groups<8;++i){
            struct json_object *base=json_object_array_get_idx(rows,i);
            const char *ip=source_ip(base),*domain=field(base,"domain");
            const char *first_date=NULL,*last_date=NULL;
            size_t j,count=0,high=0,critical=0,denied=0;
            int cross_domain=0,multiple_dates=0;
            if(visited[i]||!valid_address(ip))continue;
            for(j=i;j<n;++j){
                struct json_object *row=json_object_array_get_idx(rows,j);
                const char *other=source_ip(row),*other_domain,*date;
                int sev,status;
                if(!valid_address(other)||strcmp(ip,other))continue;
                visited[j]=1;
                ++count;
                sev=severity_of(row);
                status=status_of(row);
                if(sev==2)++high;
                if(sev==3)++critical;
                if(status==403||status==429)++denied;
                other_domain=field(row,"domain");
                if(other_domain&&*other_domain){
                    if(!domain||!*domain)domain=other_domain;
                    else if(strcmp(domain,other_domain))cross_domain=1;
                }
                date=field(row,"created_at");
                if(source_day(date)){
                    if(!first_date||strncmp(date,first_date,10)<0)first_date=date;
                    if(!last_date||strncmp(date,last_date,10)>0)last_date=date;
                }
            }
            if(first_date&&last_date&&strncmp(first_date,last_date,10))
                multiple_dates=1;
            if(count<2 || (!high&&!critical) ||
               (!cross_domain&&!multiple_dates))continue;
            ++groups;
            if(!append(out,cap,&used,
                "- **Indicator group %zu:** %zu retained source records, %zu HIGH and "
                "%zu CRITICAL classifications, %zu reported denials or rate limits. "
                "Evidence shows %s%s. **Assessment:** investigate whether the shared "
                "address represents a proxy, service provider or repeated activity; "
                "cross-check independent request and authentication records.\n",
                groups,count,high,critical,denied,
                cross_domain?"multiple reported site domains":"one reported site domain",
                multiple_dates?" across different reported source dates":""))
                { free(visited);goto finish; }
        }
        free(visited);
        if(!groups && !append(out,cap,&used,
           "No repeated HIGH/CRITICAL address indicator spanning multiple reported "
           "site domains or source dates was found in the available snapshot.\n"))
            goto finish;
    }
    if(!append(out,cap,&used,
       "\n### Evidence and investigative limitations\n\n"
       "- The journal records **first/last S2 collection**, not the source event's actual first occurrence.\n"
       "- Source-record revisions do not identify which field changed or establish impact.\n"
       "- A record repeatedly returned by the API may represent one retained event, not repeated attacks.\n"
       "- Source severity and HTTP status are investigative leads, not independent proof of compromise.\n"
       "- Determine access outcomes from independently authenticated server/application logs before concluding intrusion.\n"
       "- Investigator authority remains human-controlled; publication does not authorize blocking or remediation.\n"))
       goto finish;
    /* Do not advance the journal if the report cannot be completed. */
    if(!save_journal(&j,journal_path))goto finish;
    ok=1;
finish:
    if(root)json_object_put(root);
    if(tok)json_tokener_free(tok);
    free(j.rows);
    if(!ok)out[0]=0;
    (void)examined;
    return ok;
}
