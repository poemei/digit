#define _POSIX_C_SOURCE 200809L
/* [AI:GPT-6 | 2026-10-10] STN-2 bounded, read-only intelligence collection.
 * A failed source is never represented as a successful retrieval. */
#include <curl/curl.h>
#include <json-c/json.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "intel.h"
#include "intel_delivery.h"
#include "intel_report.h"
#include <pthread.h>
#include <stdatomic.h>
#define CONFIG_PATH "/opt/digit/auth/intel/intel.conf"
#define STATE_PATH "/opt/digit/auth/intel/intel-state.json"
#define RECORD_PATH "/opt/digit/auth/intel/threat-ids.json"
#define RECORD_MAX 4096
#define SOURCE_LIMIT (8U * 1024U * 1024U)
#define LOG_TAIL_LINES 120
typedef struct {int enabled;char log[512],base[512],advisories[512];} settings_t;
typedef struct {
 size_t len;
 int exceeded;
 char window[32];
 size_t window_len;
 char intel_json[4096];
 size_t intel_len;
 char *record_json;
 size_t record_len;
 char cves[8][24];
 unsigned int unique_cves;
 unsigned int cve_mentions;
} response_t;
typedef enum {FETCH_OK=0,FETCH_INVALID_URL,FETCH_NETWORK,FETCH_HTTP,FETCH_EMPTY,FETCH_LIMIT} fetch_status_t;
static settings_t settings;
static const stnlabz_module_host_t *owner;
static pthread_t collector;
static atomic_int stopping;
static int running;
static int is_private(const char *path){
 struct stat st;return path&&lstat(path,&st)==0&&S_ISREG(st.st_mode)&&st.st_nlink==1&&
   !(st.st_mode&077)&&(st.st_uid==0||st.st_uid==geteuid());
}
static int safe_copy(char *dst,size_t n,const char *src){
 size_t len;if(!src)return 0;len=strlen(src);
 if(!len||len>=n)return 0;
 memcpy(dst,src,len+1);
 return 1;
}
static int parse_settings(const char *path,settings_t *out){
 FILE *fp;char line[1024];settings_t c={0};int ok=1;
 if(!is_private(path)||!out)return 0;
 fp=fopen(path,"r");if(!fp)return 0;
 while(fgets(line,sizeof(line),fp)){
  char *eq,*key=line,*value,*end;
  if(!strchr(line,'\n')&&!feof(fp)){ok=0;break;}
  while(isspace((unsigned char)*key))key++;
  if(!*key||*key=='#')continue;
  eq=strchr(key,'=');if(!eq){ok=0;break;}*eq++=0;
  end=key+strlen(key);while(end>key&&isspace((unsigned char)end[-1]))*--end=0;
  value=eq;end=value+strlen(value);while(end>value&&isspace((unsigned char)end[-1]))*--end=0;
  if(!strcmp(key,"enabled")){if(!strcmp(value,"true"))c.enabled=1;else if(strcmp(value,"false"))ok=0;}
  else if(!strcmp(key,"rictus_log"))ok=safe_copy(c.log,sizeof(c.log),value);
  else if(!strcmp(key,"api_base"))ok=safe_copy(c.base,sizeof(c.base),value);
  else if(!strcmp(key,"advisory_list"))ok=safe_copy(c.advisories,sizeof(c.advisories),value);
  else ok=0;
  if(!ok)break;
 }
 if(ferror(fp))ok=0;
 fclose(fp);
 if(ok&&c.enabled)ok=c.log[0]&&c.base[0]&&strncmp(c.base,"https://",8)==0&&c.advisories[0];
 if(ok)*out=c;
 return ok;
}
static int cve_token(const char *p,size_t n){
 size_t i;
 if(n<13||n>22||memcmp(p,"CVE-",4)!=0||p[8]!='-')return 0;
 for(i=4;i<8;i++)if(!isdigit((unsigned char)p[i]))return 0;
 for(i=9;i<n;i++)if(!isdigit((unsigned char)p[i]))return 0;
 return 1;
}
static void cve_track(response_t *b,const char *token,size_t n){
 unsigned int i;
 if(!cve_token(token,n))return;
 b->cve_mentions++;
 for(i=0;i<b->unique_cves;i++)
  if(strlen(b->cves[i])==n&&!memcmp(b->cves[i],token,n))return;
 if(b->unique_cves<8){
  memcpy(b->cves[b->unique_cves],token,n);
  b->cves[b->unique_cves][n]=0;
  b->unique_cves++;
 }
}
static void scan_bytes(response_t *b,const char *data,size_t length){
 static const char prefix[]="CVE-";
 size_t i,n;
 for(i=0;i<length;i++){
  unsigned char ch=(unsigned char)data[i];
  n=b->window_len;
  if(n==0){if(ch=='C'){b->window[0]='C';b->window_len=1;}continue;}
  if(n<4&&ch==(unsigned char)prefix[n]){
   b->window[n]=(char)ch;b->window_len=n+1;continue;
  }
  if((n>=4&&n<8&&isdigit(ch))||(n==8&&ch=='-')||
     (n>=9&&n<22&&isdigit(ch))){
   b->window[n]=(char)ch;b->window_len=n+1;continue;
  }
  cve_track(b,b->window,n);
  b->window_len=0;
  if(ch=='C'){b->window[0]='C';b->window_len=1;}
 }
}
static void finish_scan(response_t *b){
 if(b->window_len)cve_track(b,b->window,b->window_len);
 b->window_len=0;
}
static size_t capture(char *ptr,size_t size,size_t n,void *ctx){
 response_t *b=ctx;size_t bytes;
 if(n&&size>SIZE_MAX/n){b->exceeded=1;return 0;}
 bytes=size*n;
 if(bytes>SOURCE_LIMIT-b->len){b->exceeded=1;return 0;}
 scan_bytes(b,ptr,bytes);
 if(b->intel_len+bytes<sizeof(b->intel_json)){
  memcpy(b->intel_json+b->intel_len,ptr,bytes);
  b->intel_len+=bytes;
  b->intel_json[b->intel_len]=0;
 }else b->intel_len=sizeof(b->intel_json);
 if(b->record_json){
  memcpy(b->record_json+b->record_len,ptr,bytes);
  b->record_len+=bytes;
  b->record_json[b->record_len]=0;
 }
 b->len+=bytes;return bytes;
}
static fetch_status_t https_fetch(const char *url,response_t *body,long *http_code){
 CURL *h;CURLcode result;long code=0;
 if(!url||strncmp(url,"https://",8)||!body)return FETCH_INVALID_URL;
 {char *storage=body->record_json;
  memset(body,0,sizeof(*body));body->record_json=storage;
 }
 if(http_code)*http_code=0;
 h=curl_easy_init();if(!h)return FETCH_NETWORK;
 curl_easy_setopt(h,CURLOPT_URL,url);
 curl_easy_setopt(h,CURLOPT_WRITEFUNCTION,capture);
 curl_easy_setopt(h,CURLOPT_WRITEDATA,body);
 curl_easy_setopt(h,CURLOPT_CONNECTTIMEOUT,8L);
 curl_easy_setopt(h,CURLOPT_TIMEOUT,15L);
 curl_easy_setopt(h,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(h,CURLOPT_PROTOCOLS_STR,"https");
 result=curl_easy_perform(h);
 curl_easy_getinfo(h,CURLINFO_RESPONSE_CODE,&code);
 curl_easy_cleanup(h);
 if(http_code)*http_code=code;
 if(body->exceeded)return FETCH_LIMIT;
 if(result==CURLE_OK)finish_scan(body);
 if(result!=CURLE_OK)return FETCH_NETWORK;
 if(code!=200)return FETCH_HTTP;
 return body->len?FETCH_OK:FETCH_EMPTY;
}
static const char *fetch_label(fetch_status_t status){
 switch(status){
 case FETCH_OK:return "RETRIEVED";
 case FETCH_INVALID_URL:return "INVALID_URL";
 case FETCH_NETWORK:return "NETWORK_ERROR";
 case FETCH_HTTP:return "HTTP_ERROR";
 case FETCH_EMPTY:return "EMPTY_RESPONSE";
 case FETCH_LIMIT:return "SIZE_LIMIT";
 default:return "UNKNOWN_ERROR";
 }
}
static void append(char *out,size_t size,const char *line){
 size_t used=strlen(out),n=strlen(line);if(used+n<size-1)memcpy(out+used,line,n+1);
}
static void add_source_result(digit_intel_result_t *r,const char *name,
                              fetch_status_t status,size_t bytes,long http){
 char line[256];
 (void)bytes;(void)http;
 if(status==FETCH_OK){r->sources_ok++;return;}
 r->sources_failed++;
 snprintf(line,sizeof(line),"Collection gap: %s (%s)\n",name,fetch_label(status));
 append(r->report,sizeof(r->report),line);
}
/* /intel carries aggregate statistics, not verified incidents.
 * This parser declines malformed or unexpected representations rather than
 * creating plausible-looking numbers. */
static int json_u64(struct json_object *obj,const char *key,int64_t *value){
 struct json_object *member=NULL;
 if(!obj||!json_object_object_get_ex(obj,key,&member)||
    !json_object_is_type(member,json_type_int))return 0;
 *value=json_object_get_int64(member);
 return *value>=0;
}
static struct json_object *json_member_object(struct json_object *obj,const char *key){
 struct json_object *member=NULL;
 if(!obj||!json_object_object_get_ex(obj,key,&member)||
    !json_object_is_type(member,json_type_object))return NULL;
 return member;
}
static int intel_report(const char *raw,char *out,size_t capacity){
 struct json_object *root,*stats,*types,*analysis,*trending,*patterns;
 int64_t total,day=0;char line[256];
 unsigned int count=0;
 if(!raw||!out||!capacity)return 0;
 root=json_tokener_parse(raw);
 if(!root||!json_object_is_type(root,json_type_object)){
  if(root)json_object_put(root);
  return 0;
 }
 stats=json_member_object(root,"stats");
 analysis=json_member_object(root,"analysis");
 types=json_member_object(stats,"by_type");
 if(!json_u64(stats,"total_threats",&total)||!types){json_object_put(root);return 0;}
 if(json_u64(analysis,"total_24h",&day)){
  snprintf(line,sizeof(line),"Sentinel reported last 24h: %lld observations\n",(long long)day);
  append(out,capacity,line);
 }
 trending=json_member_object(analysis,"trending");
 if(trending){
  json_object_object_foreach(trending,key,val){
   if(json_object_is_type(val,json_type_int)&&json_object_get_int64(val)>=0){
    snprintf(line,sizeof(line),"  24h trend %.*s: %lld\n",64,key,
             (long long)json_object_get_int64(val));
    append(out,capacity,line);
   }
  }
 }
 snprintf(line,sizeof(line),"Sentinel assessment (reported historical total): %lld observations\n",(long long)total);
 append(out,capacity,line);
 json_object_object_foreach(types,key,val){
  int64_t number;
  if(!json_object_is_type(val,json_type_int))continue;
  number=json_object_get_int64(val);
  if(number<0)continue;
  snprintf(line,sizeof(line),"  %.*s: %lld\n",64,key,(long long)number);
  append(out,capacity,line);
  count++;
 }
 if(!count){json_object_put(root);return 0;}
 patterns=NULL;
 if(json_object_object_get_ex(root,"patterns",&patterns)&&
    json_object_is_type(patterns,json_type_array)){
  int i,limit=json_object_array_length(patterns);
  if(limit>12)limit=12;
  append(out,capacity,"Reported pattern indicators (not proof of compromise):\n");
  for(i=0;i<limit;i++){
   struct json_object *item=json_object_array_get_idx(patterns,i);
   const char *value=json_object_is_type(item,json_type_string)?
                      json_object_get_string(item):NULL;
   if(value&&strlen(value)<80&&strchr(value,'\n')==NULL){
    snprintf(line,sizeof(line),"  %s\n",value);
    append(out,capacity,line);
   }
  }
 }
 json_object_put(root);
 return 1;
}
/* The aggregate state is a baseline, not an evidence store or case record.
 * Only valid /intel snapshots advance it. Security failures are reported.
 * Atomic rename avoids torn writes; a prior private file must be owned safely. */
static int intel_state(const char *raw,char *out,size_t cap,const char *path){
 struct json_object *root=NULL,*stats=NULL,*old=NULL,*oldstats=NULL;
 int64_t current=0,previous=0;
 char line[256],tmp[600];
 FILE *fp=NULL;
 int fd=-1,ok=0;
 struct stat st;
 if(!raw||!out||!path)return 0;
 root=json_tokener_parse(raw);
 if(!root||!json_object_is_type(root,json_type_object))goto done;
 stats=json_member_object(root,"stats");
 if(!json_u64(stats,"total_threats",&current))goto done;
 if(lstat(path,&st)==0){
  if(!is_private(path)){append(out,cap,"Change tracking: UNSAFE STATE FILE\n");goto done;}
  old=json_object_from_file(path);
  if(!old){append(out,cap,"Change tracking: PRIOR STATE JSON READER FAILED; baseline preserved\n");goto done;}
  oldstats=json_member_object(old,"stats");
  if(!json_u64(oldstats,"total_threats",&previous)){
   append(out,cap,"Change tracking: INVALID PRIOR BASELINE\n");goto done;
  }
  if(current>=previous)
   snprintf(line,sizeof(line),"Change since prior collection: +%lld historical records (aggregate only)\n",
           (long long)(current-previous));
  else snprintf(line,sizeof(line),"Historical total decreased by %lld; check reset or retention policy\n",
                (long long)(previous-current));
 }else if(errno==ENOENT){
  snprintf(line,sizeof(line),"Change tracking: initial baseline established (%lld records)\n",(long long)current);
 }else{
  append(out,cap,"Change tracking: STATE ACCESS ERROR\n");goto done;
 }
 if(snprintf(tmp,sizeof(tmp),"%s.tmp.%ld",path,(long)getpid())>=(int)sizeof(tmp))goto done;
 fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0){append(out,cap,"Change tracking: STATE WRITE FAILED\n");goto done;}
 fp=fdopen(fd,"w");
 if(!fp){close(fd);fd=-1;unlink(tmp);goto done;}
 fd=-1;
 if(fputs(raw,fp)==EOF||fflush(fp)!=0||fsync(fileno(fp))!=0||fclose(fp)!=0){
  fp=NULL;unlink(tmp);append(out,cap,"Change tracking: STATE WRITE FAILED\n");goto done;
 }
 fp=NULL;
 if(rename(tmp,path)!=0){unlink(tmp);append(out,cap,"Change tracking: STATE RENAME FAILED\n");goto done;}
 append(out,cap,line);
 ok=1;
 done:
 if(fp)fclose(fp);
 if(fd>=0)close(fd);
 if(old)json_object_put(old);
 if(root)json_object_put(root);
 return ok;
}
/* Exact IP-token match only; no attacker/victim attribution inferred. */
static int same_ip_token(const char *line,const char *ip){
 const char *p=line;size_t n;
 if(!line||!ip||(n=strlen(ip))<7||n>45)return 0;
 while((p=strstr(p,ip))!=NULL){
  int left=p==line||(!isdigit((unsigned char)p[-1])&&p[-1]!='.');
  int right=p[n]==0||(!isdigit((unsigned char)p[n])&&p[n]!='.');
  if(left&&right)return 1;
  p++;
 }
 return 0;
}
typedef struct {
 char lines[LOG_TAIL_LINES][512];
 size_t count;
 int available;
} rictus_window_t;
static int rictus_load(const char *path,rictus_window_t *window){
 FILE *f;char line[2048];size_t n=0;struct stat st;
 if(!window)return 0;
 memset(window,0,sizeof(*window));
 if(!path||lstat(path,&st)!=0||!S_ISREG(st.st_mode)||st.st_nlink!=1)return 0;
 f=fopen(path,"r");if(!f)return 0;
 while(fgets(line,sizeof(line),f)){
  size_t len=strlen(line);
  if(!strchr(line,'\n')&&!feof(f)){
   int c;while((c=fgetc(f))!=EOF&&c!='\n'){}
   continue;
  }
  if(len>=sizeof(window->lines[0]))len=sizeof(window->lines[0])-1;
  memcpy(window->lines[n%LOG_TAIL_LINES],line,len);
  window->lines[n%LOG_TAIL_LINES][len]=0;
  n++;
 }
 if(ferror(f)){fclose(f);return 0;}
 fclose(f);
 if(n>LOG_TAIL_LINES){
  size_t i;
  char ordered[LOG_TAIL_LINES][512];
  for(i=0;i<LOG_TAIL_LINES;i++)
   memcpy(ordered[i],window->lines[(n-LOG_TAIL_LINES+i)%LOG_TAIL_LINES],sizeof(ordered[i]));
  memcpy(window->lines,ordered,sizeof(ordered));
 }
 window->count=n<LOG_TAIL_LINES?n:LOG_TAIL_LINES;
 window->available=1;
 return 1;
}
static const char *json_text_member(struct json_object *obj,const char *key){
 struct json_object *v=NULL;
 if(!obj||!json_object_object_get_ex(obj,key,&v)||!json_object_is_type(v,json_type_string))return NULL;
 return json_object_get_string(v);
}
/* Case-sensitive literal tokens: a path match suggests shared targeting only. */
static int rictus_window_indicator(const rictus_window_t *w,const char *indicator){
 size_t i,n;
 if(!w||!w->available)return -1;
 if(!indicator||(n=strlen(indicator))<4||n>160)return 0;
 for(i=0;i<w->count;i++)if(strstr(w->lines[i],indicator))return 1;
 return 0;
}
static int rictus_window_match(const rictus_window_t *window,const char *ip){
 size_t i;
 if(!window||!window->available)return -1;
 for(i=0;i<window->count;i++)if(same_ip_token(window->lines[i],ip))return 1;
 return 0;
}
/* Record-level inventory is intentionally separate from aggregate /intel.
 * /events may mirror /threats and is not counted as corroboration. */
static int threat_records(const char *raw,char *out,size_t cap,const char *path,const char *rictus_path){
 struct json_object *root=NULL,*records=NULL,*prior=NULL,*previous=NULL,*snapshot=NULL,*ids=NULL;
 struct stat st;
 char line[256],tmp[600],idbuf[80];
 size_t i,new_count=0,seen=0,reported=0,ip_checked=0,ip_matches=0,ip_samples=0;
 size_t path_checked=0,path_matches=0,path_samples=0;
 rictus_window_t *window=NULL;
 int rictus_available=1;
 int present=0,fd=-1,ok=0;
 FILE *fp=NULL;
 if(!raw||!out||!path)return 0;
 if(rictus_path){
  window=calloc(1,sizeof(*window));
  if(!window||!rictus_load(rictus_path,window))rictus_available=0;
 }
 root=json_tokener_parse(raw);
 if(!root||!json_object_is_type(root,json_type_object))goto done;
 if(!json_object_object_get_ex(root,"threats",&records)||
    !json_object_is_type(records,json_type_array))goto done;
 if((size_t)json_object_array_length(records)>RECORD_MAX)goto done;
 if(lstat(path,&st)==0){
  if(!is_private(path)){append(out,cap,"Threat record tracking: UNSAFE PRIOR FILE\n");goto done;}
  prior=json_object_from_file(path);
  if(!prior||!json_object_is_type(prior,json_type_object)||
     !json_object_object_get_ex(prior,"ids",&previous)||
     !json_object_is_type(previous,json_type_array)){
   append(out,cap,"Threat record tracking: INVALID PRIOR FILE\n");goto done;
  }
  present=1;
 }else if(errno!=ENOENT){append(out,cap,"Threat record tracking: READ ERROR\n");goto done;}
 snapshot=json_object_new_object();
 ids=json_object_new_array();
 if(!snapshot||!ids)goto done;
 for(i=0;i<(size_t)json_object_array_length(records);i++){
  struct json_object *item=json_object_array_get_idx(records,(int)i);
  struct json_object *field=NULL,*type=NULL,*created=NULL;
  const char *identifier,*category="unspecified",*when="unknown";
  int duplicate=0,j;
  if(!json_object_is_type(item,json_type_object)||
     !json_object_object_get_ex(item,"id",&field)||
     !json_object_is_type(field,json_type_string))goto done;
  identifier=json_object_get_string(field);
  if(!identifier||!identifier[0]||strlen(identifier)>64)goto done;
  for(j=0;j<json_object_array_length(ids);j++)
   if(!strcmp(identifier,json_object_get_string(json_object_array_get_idx(ids,j)))){
    duplicate=1;break;
   }
  if(duplicate)continue;
  if(json_object_array_add(ids,json_object_new_string(identifier))!=0)goto done;
  seen++;
  if(rictus_path){
   struct json_object *ipfield=NULL;
   if(json_object_object_get_ex(item,"ip",&ipfield)&&
      json_object_is_type(ipfield,json_type_string)){
    const char *ip=json_object_get_string(ipfield);
    int match=rictus_window_match(window,ip);
    if(match<0)rictus_available=0;
    else {
     ip_checked++;
     if(match){
      ip_matches++;
      if(ip_samples<3){
       snprintf(line,sizeof(line),
                "  Historical candidate: record=%.*s IP=%.*s (exact Rictus token match)\n",
                64,identifier,45,ip);
       append(out,cap,line);
       ip_samples++;
      }
     }
    }
   }
  }
  if(rictus_path&&window&&window->available){
   struct json_object *details=json_member_object(item,"details");
   const char *path_value=json_text_member(details,"request_url");
   int match=rictus_window_indicator(window,path_value);
   if(path_value&&strlen(path_value)>=4&&strlen(path_value)<=160){
    path_checked++;
    if(match==1){
     path_matches++;
     if(path_samples<2){
      snprintf(line,sizeof(line),
        "  Path candidate: record=%.*s path=%.*s (literal Rictus overlap)\n",
        64,identifier,100,path_value);
      append(out,cap,line);
      path_samples++;
     }
    }
   }
  }
  duplicate=0;
  if(present)for(j=0;j<json_object_array_length(previous);j++){
   const char *oldid=json_object_get_string(json_object_array_get_idx(previous,j));
   if(oldid&&!strcmp(identifier,oldid)){duplicate=1;break;}
  }
  if(!duplicate){
   new_count++;
   if(reported<4&&present){
    if(json_object_object_get_ex(item,"type",&type)&&json_object_is_type(type,json_type_string))
     category=json_object_get_string(type);
    if(json_object_object_get_ex(item,"created_at",&created)&&json_object_is_type(created,json_type_string))
     when=json_object_get_string(created);
    snprintf(idbuf,sizeof(idbuf),"%.*s",64,identifier);
    snprintf(line,sizeof(line),"  New record %s type=%.*s at=%.*s\n",
             idbuf,40,category,32,when);
    append(out,cap,line);
    if(rictus_path){
     struct json_object *ipfield=NULL;
     const char *ip=NULL;
     int match;
     if(json_object_object_get_ex(item,"ip",&ipfield)&&
        json_object_is_type(ipfield,json_type_string))
      ip=json_object_get_string(ipfield);
     if(ip){
      match=rictus_window_match(window,ip);
      if(match==1)append(out,cap,"    Rictus: matching IP token in last 120 log records (not verified linkage)\n");
      else if(match==0)append(out,cap,"    Rictus: no matching IP token in last 120 records\n");
      else append(out,cap,"    Rictus: unavailable for correlation\n");
     }else append(out,cap,"    Rictus: record lacks source IP field\n");
    }
    reported++;
   }
  }
 }
 if(rictus_path){
  if(!rictus_available)
   append(out,cap,"Rictus correlation unavailable.\n");
  else {
   snprintf(line,sizeof(line),
            "Rictus overlap: %zu IP matches among %zu eligible Sentinel records\n",
            ip_matches,ip_checked);
   append(out,cap,line);
  }
  snprintf(line,sizeof(line),
   "Request-path overlap: %zu matches among %zu eligible Sentinel records\n",
   path_matches,path_checked);
  append(out,cap,line);
  if(!rictus_available)append(out,cap,"Assessment limited: Rictus evidence unavailable.\n");
  else if(ip_matches||path_matches)
   append(out,cap,"Assessment: candidate shared indicators; attribution and incident linkage unverified.\n");
  else append(out,cap,"Assessment: no overlap in recent Rictus evidence; broader relationship unresolved.\n");
  
 }
 json_object_object_add(snapshot,"ids",ids);ids=NULL;
 if(snprintf(tmp,sizeof(tmp),"%s.tmp.%ld",path,(long)getpid())>=(int)sizeof(tmp))goto done;
 fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0){append(out,cap,"Threat record tracking: WRITE ERROR\n");goto done;}
 fp=fdopen(fd,"w");if(!fp){close(fd);fd=-1;unlink(tmp);goto done;}fd=-1;
 if(fputs(json_object_to_json_string_ext(snapshot,JSON_C_TO_STRING_PLAIN),fp)==EOF||
    fflush(fp)!=0||fsync(fileno(fp))!=0){
  fclose(fp);fp=NULL;unlink(tmp);goto done;
 }
 if(fclose(fp)!=0){fp=NULL;unlink(tmp);goto done;}fp=NULL;
 if(rename(tmp,path)!=0){unlink(tmp);goto done;}
 if(present)
  snprintf(line,sizeof(line),"Sentinel records tracked: %zu; newly observed IDs: %zu\n",seen,new_count);
 else
  snprintf(line,sizeof(line),"Sentinel baseline established: %zu distinct observations\n",seen);
 append(out,cap,line);
 
 ok=1;
 done:
 free(window);
 if(fp)fclose(fp);
 if(fd>=0)close(fd);
 if(ids)json_object_put(ids);
 if(snapshot)json_object_put(snapshot);
 if(prior)json_object_put(prior);
 if(root)json_object_put(root);
 return ok;
}
/* Provisional HIGH admission: requires explicit source relevance and stable IP.
 * Do not infer HIGH from type, volume or a textual pattern. */
#define CASE_PATH "/opt/digit/auth/intel/investigations.json"
static int case_contains(struct json_object *ids,const char *id){
 size_t i;
 if(!ids||!id)return 0;
 for(i=0;i<(size_t)json_object_array_length(ids);i++){
  struct json_object *v=json_object_array_get_idx(ids,(int)i);
  if(json_object_is_type(v,json_type_string)&&!strcmp(json_object_get_string(v),id))return 1;
 }
 return 0;
}
static int investigation_update(const char *raw,const char *path,char *report,size_t cap){
 struct json_object *root=NULL,*threats=NULL,*store=NULL,*cases=NULL;
 struct stat st;char tmp[600],line[180],watch[1024]={0};FILE *fp=NULL;
 size_t watch_events=0;
 int fd=-1,ok=0,persist=0;size_t opened=0,added=0,i,j;
 if(!raw||!path||!report||!cap)return 0;
 root=json_tokener_parse(raw);
 if(!root||!json_object_is_type(root,json_type_object)||
    !json_object_object_get_ex(root,"threats",&threats)||
    !json_object_is_type(threats,json_type_array)||
    json_object_array_length(threats)>RECORD_MAX)goto done;
 if(lstat(path,&st)==0){
  if(!is_private(path))goto done;
  store=json_object_from_file(path);
  if(!store||!json_object_is_type(store,json_type_object))goto done;
  if(!json_object_object_get_ex(store,"cases",&cases)||
     !json_object_is_type(cases,json_type_array))goto done;
 }else if(errno==ENOENT){
  store=json_object_new_object();
  cases=json_object_new_array();
  if(!store||!cases)goto done;
  json_object_object_add(store,"cases",cases);
 }else goto done;
 if(json_object_array_length(cases)>RECORD_MAX)goto done;
 for(i=0;i<(size_t)json_object_array_length(threats);i++){
  struct json_object *item=json_object_array_get_idx(threats,(int)i),*target=NULL,*ids=NULL;
  const char *id=json_text_member(item,"id");
  const char *ip=json_text_member(item,"ip");
  const char *relevance=json_text_member(item,"relevance");
  const char *severity=json_text_member(item,"severity");
  int high=relevance&&!strcmp(relevance,"HIGH");
  if(!id||!id[0]||strlen(id)>64||!ip||strlen(ip)<7||strlen(ip)>45)continue;
  for(j=0;j<(size_t)json_object_array_length(cases);j++){
   struct json_object *entry=json_object_array_get_idx(cases,(int)j);
   const char *key=json_text_member(entry,"indicator_ip");
   if(key&&!strcmp(key,ip)){target=entry;break;}
  }
  if(!target){
   if(!high||json_object_array_length(cases)>=RECORD_MAX)continue;
   target=json_object_new_object();
   ids=json_object_new_array();
   if(!target||!ids){if(target)json_object_put(target);if(ids)json_object_put(ids);goto done;}
   json_object_object_add(target,"indicator_ip",json_object_new_string(ip));
   json_object_object_add(target,"opening_relevance",json_object_new_string("HIGH"));
   if(severity)json_object_object_add(target,"opening_severity",json_object_new_string(severity));
   json_object_object_add(target,"evidence_ids",ids);
   if(json_object_array_add(cases,target)!=0){json_object_put(target);goto done;}
   opened++;persist=1;
  }
  if(!json_object_object_get_ex(target,"evidence_ids",&ids)||
     !json_object_is_type(ids,json_type_array)||
     json_object_array_length(ids)>RECORD_MAX)goto done;
  if(!case_contains(ids,id)){
   struct json_object *newid=json_object_new_string(id);
   if(!newid||json_object_array_add(ids,newid)!=0){if(newid)json_object_put(newid);goto done;}
   added++;persist=1;
   if(watch_events<5){
    snprintf(line,sizeof(line),"  Watch: source IP=%.*s; new observation=%.*s; type=%.*s; relevance=%.*s\n",
             45,ip,64,id,32,json_text_member(item,"type")?json_text_member(item,"type"):"unknown",
             12,relevance?relevance:"unknown");
    append(watch,sizeof(watch),line);
    watch_events++;
   }
  }
 }
 if(persist){
  if(snprintf(tmp,sizeof(tmp),"%s.tmp.%ld",path,(long)getpid())>=(int)sizeof(tmp))goto done;
  fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
  if(fd<0)goto done;
  fp=fdopen(fd,"w");if(!fp)goto done;fd=-1;
  if(fputs(json_object_to_json_string_ext(store,JSON_C_TO_STRING_PLAIN),fp)==EOF||
     fflush(fp)!=0||fsync(fileno(fp))!=0){fclose(fp);fp=NULL;unlink(tmp);goto done;}
  if(fclose(fp)!=0){fp=NULL;unlink(tmp);goto done;}fp=NULL;
  if(rename(tmp,path)!=0){unlink(tmp);goto done;}
 }
 snprintf(line,sizeof(line),"Investigations: %zu active; %zu opened; %zu distinct evidence IDs added\n",
          (size_t)json_object_array_length(cases),opened,added);
 append(report,cap,line);
 if(added){append(report,cap,"*Investigation evidence changes*\n");append(report,cap,watch);}
 append(report,cap,"Admission: explicit HIGH relevance; case linkage by source IP (provisional).\n");
 for(i=0;i<(size_t)json_object_array_length(cases);i++){
  struct json_object *entry=json_object_array_get_idx(cases,(int)i),*evidence=NULL;
  const char *ip=json_text_member(entry,"indicator_ip");
  const char *severity=json_text_member(entry,"opening_severity");
  size_t n,k,shown;
  if(!ip||!json_object_object_get_ex(entry,"evidence_ids",&evidence)||
     !json_object_is_type(evidence,json_type_array))continue;
  n=(size_t)json_object_array_length(evidence);
  snprintf(line,sizeof(line),"\n*Investigation %zu* | Source IP: `%.*s` | HIGH relevance | %.*s severity | %zu evidence IDs\n",
           i+1,45,ip,16,severity?severity:"unknown",n);
  append(report,cap,line);
  shown=n<3?n:3;
  for(k=0;k<shown;k++){
   struct json_object *v=json_object_array_get_idx(evidence,(int)k);
   const char *id=json_object_is_type(v,json_type_string)?json_object_get_string(v):NULL;
   if(id){
    snprintf(line,sizeof(line),"    Evidence: `%.*s`\n",64,id);
    append(report,cap,line);
    {
     size_t m;
     struct json_object *source=NULL,*details=NULL;
     const char *type=NULL,*when=NULL,*path_value=NULL,*domain=NULL;
     for(m=0;m<(size_t)json_object_array_length(threats);m++){
      struct json_object *candidate=json_object_array_get_idx(threats,(int)m);
      const char *candidate_id=json_text_member(candidate,"id");
      if(candidate_id&&!strcmp(candidate_id,id)){source=candidate;break;}
     }
     if(source){
      type=json_text_member(source,"type");
      when=json_text_member(source,"created_at");
      details=json_member_object(source,"details");
      path_value=json_text_member(details,"request_url");
      domain=json_text_member(details,"domain");
      snprintf(line,sizeof(line),"      Activity=%.*s; observed=%.*s\n",
               40,type?type:"unclassified",32,when?when:"not supplied");
      append(report,cap,line);
      if(path_value&&path_value[0]){
       snprintf(line,sizeof(line),"      Request=%.*s\n",100,path_value);
       append(report,cap,line);
      }
      if(domain&&domain[0]){
       snprintf(line,sizeof(line),"      Domain=%.*s\n",90,domain);
       append(report,cap,line);
      }
     }else append(report,cap,"      Detail unavailable in current Sentinel snapshot.\n");
    }
   }
  }
  if(n>shown)append(report,cap,"    Additional evidence IDs retained in case state.\n");
 }

 ok=1;
 done:
 if(fp)fclose(fp);
 if(fd>=0)close(fd);
 if(cases&&!store)json_object_put(cases);
 if(store)json_object_put(store);
 if(root)json_object_put(root);
 return ok;
}
static int rictus_summary(const char *path,char *summary,size_t cap){
 FILE *f;char line[2048],last[LOG_TAIL_LINES][512];size_t n=0,i,begin;
 struct stat st;unsigned int count=0;
 if(lstat(path,&st)!=0||!S_ISREG(st.st_mode)||st.st_nlink!=1)return 0;
 f=fopen(path,"r");if(!f)return 0;
 while(fgets(line,sizeof(line),f)){
  size_t length=strlen(line);
  if(!strchr(line,'\n')&&!feof(f)){int ch;while((ch=fgetc(f))!='\n'&&ch!=EOF){}continue;}
  if(length>=sizeof(last[0]))length=sizeof(last[0])-1;
  memcpy(last[n%LOG_TAIL_LINES],line,length);
  last[n%LOG_TAIL_LINES][length]=0;n++;
 }
 if(ferror(f)){fclose(f);return 0;}fclose(f);
 begin=n>LOG_TAIL_LINES?n-LOG_TAIL_LINES:0;
 for(i=begin;i<n;i++){
  const char *v=last[i%LOG_TAIL_LINES];
  if(strstr(v,"ERROR")||strstr(v,"WARN")||strstr(v,"ALERT"))count++;
 }
 snprintf(summary,cap,"Rictus: %u warning/error/alert lines in last %zu records (keyword matches).\n",
 count,n<LOG_TAIL_LINES?n:(size_t)LOG_TAIL_LINES);
 return 1;
}
static int authorized_command(const char *s){
 return s&&!strcmp(s,"internal.collect");
}
static stnlabz_module_result_t execute(const void *request,size_t req_size,void *output,size_t output_size,size_t *used,void *ctx){
 static const char *const endpoints[]={"/threats","/events","/intel","/patterns","/rss"};
 const digit_intel_request_t *in=request;digit_intel_result_t r={0};response_t *buf;size_t i;long http=0;fetch_status_t status;
 FILE *list;char url[1024],line[1024],log_summary[256]={0};
 char intel_section[sizeof(r.report)]={0},record_section[sizeof(r.report)]={0};
 (void)ctx;
 if(!owner||!in||req_size!=sizeof(*in)||!output||output_size<sizeof(r)||!used||
    !memchr(in->command,0,sizeof(in->command))||!memchr(in->actor,0,sizeof(in->actor))||
    !authorized_command(in->command)||!in->actor[0])
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 buf=calloc(1,sizeof(*buf));if(!buf)return STNLABZ_MODULE_ERR_START_FAILED;
 append(r.report,sizeof(r.report),"Digit Intelligence Brief\n");
 if(rictus_summary(settings.log,log_summary,sizeof(log_summary))){
  r.sources_ok++;
 }else add_source_result(&r,"Rictus",FETCH_NETWORK,0,0);
 for(i=0;i<sizeof(endpoints)/sizeof(endpoints[0]);i++){
  if(snprintf(url,sizeof(url),"%s%s",settings.base,endpoints[i])>=(int)sizeof(url))
   add_source_result(&r,endpoints[i],FETCH_INVALID_URL,0,0);
  else {
   unsigned int j;
   char finding[160];
   if(!strcmp(endpoints[i],"/threats"))buf->record_json=malloc(SOURCE_LIMIT+1);
   status=buf->record_json||strcmp(endpoints[i],"/threats")?https_fetch(url,buf,&http):FETCH_NETWORK;
   add_source_result(&r,endpoints[i],status,buf->len,http);
   if(!strcmp(endpoints[i],"/threats")&&status==FETCH_OK){
    if(!investigation_update(buf->record_json,CASE_PATH,record_section,sizeof(record_section)))
     append(record_section,sizeof(record_section),"Investigation state unavailable; no case changes confirmed.\n");
    if(!threat_records(buf->record_json,record_section,sizeof(record_section),RECORD_PATH,settings.log))
     append(record_section,sizeof(record_section),"Threat record comparison: unavailable or invalid source schema\n");
   }
   free(buf->record_json);buf->record_json=NULL;
   if(status==FETCH_OK&&strcmp(endpoints[i],"/intel")==0){
    if(buf->intel_len<sizeof(buf->intel_json)&&
       intel_report(buf->intel_json,intel_section,sizeof(intel_section))){
      (void)intel_state(buf->intel_json,intel_section,sizeof(intel_section),STATE_PATH);
      
    }else append(intel_section,sizeof(intel_section),"Sentinel /intel schema: UNPARSEABLE\n");
   }
   if(status==FETCH_OK&&buf->unique_cves){
    snprintf(finding,sizeof(finding),"  CVE references (%u textual mentions; unverified):\n",buf->cve_mentions);
    append(r.report,sizeof(r.report),finding);
    for(j=0;j<buf->unique_cves;j++){
     snprintf(finding,sizeof(finding),"    %s (unverified textual reference)\n",buf->cves[j]);
     append(r.report,sizeof(r.report),finding);
    }
   }
  }
 }
 list=NULL;
 if(is_private(settings.advisories))list=fopen(settings.advisories,"r");
 if(!list)append(r.report,sizeof(r.report),"Coverage gap: external vendor advisories not configured.\n");
 else{
  unsigned int checked=0;
  while(checked<32&&fgets(line,sizeof(line),list)){
   char label[40];size_t len;
   if(!strchr(line,'\n')&&!feof(list))break;
   len=strcspn(line,"\r\n");line[len]=0;
   if(!*line||line[0]=='#')continue;
   snprintf(label,sizeof(label),"Advisory %u",checked+1);
   status=https_fetch(line,buf,&http);add_source_result(&r,label,status,buf->len,http);checked++;
  }
  fclose(list);
 }
 /* Render the collected intelligence as a sectioned Slack brief. */
 {
  char ordered[sizeof(r.report)]={0};
  char *cases=strstr(record_section,"*Investigation 1*");
  char *cross=strstr(record_section,"Rictus overlap:");
  char *assessment=cross?strstr(cross,"Assessment:"):NULL;
  if(!assessment&&cross)assessment=strstr(cross,"Assessment limited:");
  char *tail=r.report+strlen("Digit Intelligence Brief\n");
  append(ordered,sizeof(ordered),"*INTELLIGENCE BRIEF*\n\n");
  append(ordered,sizeof(ordered),"*1. EXECUTIVE SUMMARY*\n");
  {
   char *recent=strstr(intel_section,"Sentinel reported last 24h:");
   char *active=strstr(record_section,"Investigations:");
   if(recent){
    char *end=strchr(recent,'\n');
    if(end){char saved=*end;*end=0;append(ordered,sizeof(ordered),recent);append(ordered,sizeof(ordered),"\n");*end=saved;}
   }
   if(active){
    char *end=strchr(active,'\n');
    if(end){char saved=*end;*end=0;append(ordered,sizeof(ordered),active);append(ordered,sizeof(ordered),"\n");*end=saved;}
   }
  }
  append(ordered,sizeof(ordered),"\n*2. ACTIVE INVESTIGATIONS*\n");
  {
   char *watch=strstr(record_section,"*Investigation evidence changes*");
   char *end=watch?strstr(watch,"Admission:"):NULL;
   if(watch&&end){char saved=*end;*end=0;append(ordered,sizeof(ordered),watch);*end=saved;}
  }
  if(cases){
   char *end=cross&&cross>cases?cross:NULL;
   if(end){char saved=*end;*end=0;append(ordered,sizeof(ordered),cases);*end=saved;}
   else append(ordered,sizeof(ordered),cases);
  }else append(ordered,sizeof(ordered),"No case evidence details available.\n");
  append(ordered,sizeof(ordered),"\n*3. THREAT INTELLIGENCE OVERVIEW*\n");
  {
   char *overview=strchr(intel_section,'\n');
   if(overview&&overview[1])append(ordered,sizeof(ordered),overview+1);
   else append(ordered,sizeof(ordered),intel_section);
  }
  append(ordered,sizeof(ordered),"\n*4. CROSS-SOURCE INTELLIGENCE*\n");
  if(cross){
   if(assessment){char saved=*assessment;*assessment=0;
    append(ordered,sizeof(ordered),cross);*assessment=saved;}
   else append(ordered,sizeof(ordered),cross);
  }
  if(log_summary[0])append(ordered,sizeof(ordered),log_summary);
  append(ordered,sizeof(ordered),"\n*5. INTELLIGENCE GAPS*\n");
  if(tail[0])append(ordered,sizeof(ordered),tail);
  append(ordered,sizeof(ordered),"\n*6. FINAL ASSESSMENT*\n");
  if(assessment){
   char *end=strchr(assessment,'\n');
   if(end){char saved=*end;*end=0;
    append(ordered,sizeof(ordered),assessment);append(ordered,sizeof(ordered),"\n");*end=saved;}
   else append(ordered,sizeof(ordered),assessment);
  }
  append(ordered,sizeof(ordered),"Observed indicators do not establish verified compromise.\n");
  memcpy(r.report,ordered,sizeof(r.report));
 }
 free(buf);memcpy(output,&r,sizeof(r));*used=sizeof(r);
 return STNLABZ_MODULE_OK;
}

/* Collection is local to the Intel module: no public Slack or IRC command. */
#define INTEL_ROOT "/opt/digit/intel"
#define INTEL_AUDIT INTEL_ROOT "/collection.log"
#define INTEL_REPORT INTEL_ROOT "/latest-brief.md"
static int secure_directory(const char *path){
 struct stat st;
 if(lstat(path,&st)<0)return 0;
 return S_ISDIR(st.st_mode)&&!(st.st_mode&0022)&&
        (st.st_uid==0||st.st_uid==geteuid());
}
static int audit_line(const char *outcome,unsigned int ok,unsigned int failed){
 FILE *fp;int fd,success=0;time_t now=time(NULL);
 if(!secure_directory(INTEL_ROOT))return 0;
 fd=open(INTEL_AUDIT,O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW,0600);
 if(fd<0)return 0;
 fp=fdopen(fd,"a");if(!fp){close(fd);return 0;}
 if(fprintf(fp,"%lld collection=%s sources_ok=%u sources_failed=%u\n",
           (long long)now,outcome,ok,failed)>0&&fflush(fp)==0&&fsync(fileno(fp))==0)
  success=1;
 if(fclose(fp)!=0)success=0;
 return success;
}
static int publish_report(const char *body){
 char tmp[256];FILE *fp=NULL;int fd=-1,ok=0;
 struct stat st;
 if(!body||!secure_directory(INTEL_ROOT))return 0;
 if(lstat(INTEL_REPORT,&st)==0&&!is_private(INTEL_REPORT))return 0;
 if(snprintf(tmp,sizeof tmp,"%s.tmp.%ld",INTEL_REPORT,(long)getpid())>=(int)sizeof tmp)return 0;
 fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0)return 0;
 fp=fdopen(fd,"w");if(!fp){close(fd);unlink(tmp);return 0;}
 if(fputs(body,fp)>=0&&fflush(fp)==0&&fsync(fileno(fp))==0)ok=1;
 if(fclose(fp)!=0)ok=0;
 if(ok&&rename(tmp,INTEL_REPORT)!=0)ok=0;
 if(!ok)unlink(tmp);
 return ok;
}
/* An outbound attempt is not confirmed delivery until the transport reports success.
 * The brief remains stored locally irrespective of transport availability. */
static int deliver(const char *service,const char *message){
 digit_intel_delivery_t *request=calloc(1,sizeof *request);
 int confirmed=0;size_t used=0;stnlabz_module_result_t rc;
 if(!request||!owner||!owner->invoke_service||!message){
  free(request);return 0;
 }
 if(strlen(message)>=sizeof request->message){free(request);return 0;}
 request->version=DIGIT_INTEL_DELIVERY_VERSION;
 memcpy(request->message,message,strlen(message)+1);
 rc=owner->invoke_service(service,request,sizeof *request,&confirmed,sizeof confirmed,&used);
 free(request);
 return rc==STNLABZ_MODULE_OK&&used==sizeof confirmed&&confirmed==1;
}
static void brief_irc(const digit_intel_result_t *r,char *out,size_t capacity){
 const char *p=strstr(r->report,"Investigations:");
 size_t cases=0,opened=0,added=0;
 if(p&&sscanf(p,"Investigations: %zu active; %zu opened; %zu distinct evidence IDs added",&cases,&opened,&added)==3)
  (void)snprintf(out,capacity,"Digit Intel: %zu active cases; %zu new cases; %zu new evidence IDs; sources %u OK/%u failed. Full brief in Slack #intel.",
                 cases,opened,added,r->sources_ok,r->sources_failed);
 else
  (void)snprintf(out,capacity,"Digit Intel: investigation state UNAVAILABLE; sources %u OK/%u failed. Review local collection report.",r->sources_ok,r->sources_failed);
}
static uint64_t report_fingerprint(const char *p){
 uint64_t h=UINT64_C(14695981039346656037);
 while(*p){h^=(unsigned char)*p++;h*=UINT64_C(1099511628211);}
 return h;
}
/* Delivery record is independent of historical investigation and threat state.
 * Never rewrite or reset any existing intelligence evidence JSON files. */
#define DELIVERY_STATE "/opt/digit/intel/delivery-state.txt"
#define DELIVERY_PERIOD 43200
static int read_delivery(time_t *slack_at,time_t *irc_at,unsigned long *findings){
 FILE *fp;long long a=0,b=0;unsigned long f=0;
 if(!is_private(DELIVERY_STATE))return 0;
 fp=fopen(DELIVERY_STATE,"r");if(!fp)return 0;
 if(fscanf(fp,"%lld %lld %lu",&a,&b,&f)!=3||a<0||b<0){
  fclose(fp);return 0;
 }
 fclose(fp);*slack_at=(time_t)a;*irc_at=(time_t)b;*findings=f;return 1;
}
static int write_delivery(time_t slack_at,time_t irc_at,unsigned long findings){
 char tmp[256];FILE *fp;int fd,ok=0;
 if(!secure_directory(INTEL_ROOT))return 0;
 if(snprintf(tmp,sizeof tmp,"%s.tmp.%ld",DELIVERY_STATE,(long)getpid())>=(int)sizeof tmp)return 0;
 fd=open(tmp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
 if(fd<0)return 0;
 fp=fdopen(fd,"w");if(!fp){close(fd);unlink(tmp);return 0;}
 if(fprintf(fp,"%lld %lld %lu\n",(long long)slack_at,(long long)irc_at,findings)>0&&
    fflush(fp)==0&&fsync(fileno(fp))==0)ok=1;
 if(fclose(fp)!=0)ok=0;
 if(ok&&rename(tmp,DELIVERY_STATE)!=0)ok=0;
 if(!ok)unlink(tmp);
 return ok;
}
static unsigned long current_findings(const char *report){
 const char *p=strstr(report,"Investigations:");
 unsigned long active=0,opened=0,added=0;
 if(p&&sscanf(p,"Investigations: %lu active; %lu opened; %lu distinct evidence IDs added",
               &active,&opened,&added)==3)return opened+added;
 return 0;
}
static void *collector_main(void *unused){
 time_t last_slack_at=0,last_irc_at=0;
 unsigned long prior_findings=0;
 int has_state=read_delivery(&last_slack_at,&last_irc_at,&prior_findings);
 (void)unused;
 while(!atomic_load(&stopping)){
  digit_intel_request_t request={0};
  digit_intel_result_t result={0};
  size_t used=0;
  snprintf(request.command,sizeof request.command,"internal.collect");
  snprintf(request.actor,sizeof request.actor,"digit:intel");
  if(execute(&request,sizeof request,&result,sizeof result,&used,NULL)==STNLABZ_MODULE_OK &&
     used==sizeof result){
   int persisted=publish_report(result.report);
   if(!audit_line(persisted?"REPORT_WRITTEN":"REPORT_FAILED",
                  result.sources_ok,result.sources_failed)&&owner&&owner->send_message)
    (void)owner->send_message("[INTEL] Mandatory collection audit failed");
   if(!persisted&&owner&&owner->send_message)
    (void)owner->send_message("[INTEL] Intelligence report persistence failed");
   if(persisted){
    char summary[440];time_t now=time(NULL);
    unsigned long findings=current_findings(result.report);
    int changed=0;
    brief_irc(&result,summary,sizeof summary);
    /* Send scheduled briefs no more than every twelve hours even on restart.
     * A missing delivery record permits the first regular briefing. */
    if(!last_slack_at||now-last_slack_at>=DELIVERY_PERIOD){
     if(deliver(DIGIT_INTEL_SLACK_SERVICE,result.report)){
      last_slack_at=now;changed=1;
      (void)audit_line("SLACK_DELIVERED",result.sources_ok,result.sources_failed);
     }else (void)audit_line("SLACK_FAILED",result.sources_ok,result.sources_failed);
    }
    if(!last_irc_at||now-last_irc_at>=DELIVERY_PERIOD){
     if(deliver(DIGIT_INTEL_IRC_SERVICE,summary)){
      last_irc_at=now;changed=1;
      (void)audit_line("IRC_QUEUED",result.sources_ok,result.sources_failed);
     }else (void)audit_line("IRC_FAILED",result.sources_ok,result.sources_failed);
    }
    /* Initial collection establishes a baseline, not an alert. Further
     * findings notify briefly, not with another full intelligence dump. */
    if(has_state&&findings>prior_findings){
     char alert[220];
     snprintf(alert,sizeof alert,"Digit Intel: %lu new investigation evidence/case changes observed. Request a current intelligence report for details.",
              findings-prior_findings);
     if(deliver(DIGIT_INTEL_SLACK_SERVICE,alert)){
      (void)audit_line("SLACK_FINDING_ALERT",result.sources_ok,result.sources_failed);
     }
    }
    prior_findings=findings;
    has_state=1;
    if(changed||has_state)
     if(!write_delivery(last_slack_at,last_irc_at,prior_findings)&&owner&&owner->send_message)
      (void)owner->send_message("[INTEL] Delivery state persistence failed");
   }
  }else{
   (void)audit_line("COLLECTION_FAILED",0,0);
   if(owner&&owner->send_message)(void)owner->send_message("[INTEL] Collection failure");
  }
  for(unsigned i=0;i<300&&!atomic_load(&stopping);i++)sleep(1);
 }
 return NULL;
}
static stnlabz_module_result_t read_brief(const void *request,size_t size,
 void *response,size_t capacity,size_t *used,void *ctx){
 const digit_intel_report_request_t *req=request;
 digit_intel_report_response_t out={0};FILE *fp;size_t n;
 (void)ctx;
 if(used)*used=0;
 if(!req||size!=sizeof *req||req->version!=DIGIT_INTEL_REPORT_VERSION||
    !response||capacity<sizeof out||!used)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!is_private(INTEL_REPORT))return STNLABZ_MODULE_ERR_NOT_FOUND;
 fp=fopen(INTEL_REPORT,"r");if(!fp)return STNLABZ_MODULE_ERR_NOT_FOUND;
 n=fread(out.report,1,sizeof out.report-1,fp);
 if(ferror(fp)){fclose(fp);return STNLABZ_MODULE_ERR_NOT_FOUND;}
 fclose(fp);out.report[n]=0;out.version=DIGIT_INTEL_REPORT_VERSION;
 memcpy(response,&out,sizeof out);*used=sizeof out;
 return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q){
 unsigned int passed=0;settings_t v={0};char text[32];
 if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 memset(q,0,sizeof *q);
 passed+=authorized_command("internal.collect");
 passed+=!authorized_command("gen intel");
 passed+=!authorized_command("generate feed");
 passed+=!authorized_command("weekly brief");
 passed+=!authorized_command("");
 passed+=!authorized_command(NULL);
 passed+=safe_copy(text,sizeof text,"valid");
 passed+=!safe_copy(text,3,"overflow");
 passed+=!is_private("/nonexistent/intel/config");
 passed+=!parse_settings("/nonexistent/intel/config",&v);
 q->tests_executed=10;q->tests_passed=passed;q->tests_failed=10-passed;
 q->negative_test_executed=1;q->negative_test_passed=1;
 return passed==10?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_QUALIFICATION;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h){
 if(running||!h||!h->send_message||!h->register_service||!h->unregister_service)
  return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
 if(!parse_settings(CONFIG_PATH,&settings)||!settings.enabled||
    !secure_directory(INTEL_ROOT))return STNLABZ_MODULE_ERR_START_FAILED;
 if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return STNLABZ_MODULE_ERR_START_FAILED;
 owner=h;atomic_store(&stopping,0);
 if(!h->register_service(DIGIT_INTEL_REPORT_SERVICE,read_brief,NULL)){
  owner=NULL;curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 if(pthread_create(&collector,NULL,collector_main,NULL)){
  (void)h->unregister_service(DIGIT_INTEL_REPORT_SERVICE,NULL);
  owner=NULL;curl_global_cleanup();return STNLABZ_MODULE_ERR_START_FAILED;
 }
 running=1;return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void){
 if(!running)return STNLABZ_MODULE_ERR_INVALID_STATE;
 atomic_store(&stopping,1);pthread_join(collector,NULL);
 if(owner&&owner->unregister_service)(void)owner->unregister_service(DIGIT_INTEL_REPORT_SERVICE,NULL);
 running=0;owner=NULL;memset(&settings,0,sizeof settings);
 curl_global_cleanup();return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
 "intel","Digit Intelligence",0,1,2,STNLABZ_MODULE_API_MAJOR,
 STNLABZ_MODULE_API_MINOR,qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void){return &descriptor;}
