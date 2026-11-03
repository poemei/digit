#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "github_module.h"

/* Read-only public GitHub repository inspection. No local git, shell, or write HTTP methods. */
#ifndef GITHUB_ALLOWLIST_PATH
#define GITHUB_ALLOWLIST_PATH "/opt/digit/modules/github/repos.conf"
#endif
#define API "https://api.github.com/repos/"
#define MAX_BODY (1024U*1024U)
#define MAX_APPROVED 64U
#define MAX_REPORT 8192U

typedef struct { char owner[80], repo[80]; } approved_t;
typedef struct { char bytes[MAX_BODY+1]; size_t size; } response_t;
static approved_t allowed[MAX_APPROVED];
static size_t allowed_count;
static const stnlabz_module_host_t *host_ref;
static int command_registered,service_registered;

static int segment(const char *s,size_t max)
{
    size_t n=0;
    if(!s||!s[0])return 0;
    for(;s[n];++n){
        unsigned char c=(unsigned char)s[n];
        if(n>=max || !(isalnum(c)||c=='-'||c=='_'||c=='.'))return 0;
    }
    return s[0]!='.' && n<=max;
}
static int repository(const char *s,char owner[80],char repo[80])
{
    const char *slash;
    size_t n,m;
    if(!s || !(slash=strchr(s,'/')) || strchr(slash+1,'/'))return 0;
    n=(size_t)(slash-s);m=strlen(slash+1);
    if(!n||n>=80||!m||m>=80)return 0;
    memcpy(owner,s,n);owner[n]=0;
    memcpy(repo,slash+1,m+1);
    return segment(owner,79)&&segment(repo,79);
}
static int path_allowed(const char *s)
{
    size_t i=0,part=0;
    if(!s||!*s||*s=='/')return 0;
    for(;s[i];++i){
        unsigned char c=(unsigned char)s[i];
        if(i>=300)return 0;
        if(c=='/'){
            if(part==0)return 0;
            part=0;
        }else if(isalnum(c)||c=='-'||c=='_'||c=='.'){
            ++part;
        }else return 0;
    }
    if(!part||strstr(s,"../")||strstr(s,"/..")||!strncmp(s,"..",2))
        return 0;
    return 1;
}
static int permit(const char *owner,const char *repo)
{
    size_t i;
    for(i=0;i<allowed_count;++i)
        if(!strcmp(allowed[i].owner,owner)&&!strcmp(allowed[i].repo,repo))
            return 1;
    return 0;
}
static int load_allowlist(const char *path)
{
    FILE *f;
    char line[240];
    approved_t entries[MAX_APPROVED];
    size_t count=0,i;
    f=fopen(path,"r");
    if(!f)return 0;
    while(fgets(line,sizeof line,f)){
        char *p=line,*end;
        if(!strchr(line,'\n') && !feof(f)){fclose(f);return 0;}
        while(isspace((unsigned char)*p))++p;
        if(!*p||*p=='#')continue;
        end=p+strlen(p);
        while(end>p&&isspace((unsigned char)end[-1]))*--end=0;
        if(count>=MAX_APPROVED||
           !repository(p,entries[count].owner,entries[count].repo)){
            fclose(f);return 0;
        }
        for(i=0;i<count;++i)
            if(!strcmp(entries[i].owner,entries[count].owner) &&
               !strcmp(entries[i].repo,entries[count].repo)){
                fclose(f);return 0;
            }
        ++count;
    }
    if(ferror(f)||fclose(f)!=0||!count)return 0;
    memcpy(allowed,entries,count*sizeof entries[0]);
    allowed_count=count;
    return 1;
}
static size_t receive(char *bytes,size_t one,size_t count,void *arg)
{
    response_t *r=arg;
    size_t n;
    if(!one||count>MAX_BODY/one)return 0;
    n=one*count;
    if(n>MAX_BODY-r->size)return 0;
    memcpy(r->bytes+r->size,bytes,n);
    r->size+=n;
    r->bytes[r->size]=0;
    return n;
}
static int github_get(const char *endpoint,response_t *r)
{
    CURL *curl;
    CURLcode result;
    long status=0;
    char url[1200];
    struct curl_slist *headers=NULL;
    int ok=0;
    if(!endpoint||!r||
       snprintf(url,sizeof url,"%s%s",API,endpoint)>=(int)sizeof url)return 0;
    memset(r,0,sizeof *r);
    curl=curl_easy_init();
    if(!curl)return 0;
    headers=curl_slist_append(headers,"Accept: application/vnd.github+json");
    headers=curl_slist_append(headers,"X-GitHub-Api-Version: 2022-11-28");
    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"Digit-GitHub-ReadOnly/1.0");
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,15L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,r);
    result=curl_easy_perform(curl);
    if(result==CURLE_OK)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
    ok=(result==CURLE_OK&&status==200L&&r->size);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return ok;
}
static struct json_object *decode(const response_t *r,enum json_type type)
{
    struct json_tokener *tok;
    struct json_object *root;
    if(!r||!r->size)return NULL;
    tok=json_tokener_new();
    if(!tok)return NULL;
    root=json_tokener_parse_ex(tok,r->bytes,(int)r->size);
    if(json_tokener_get_error(tok)!=json_tokener_success ||
       !root||!json_object_is_type(root,type)){
        if(root)json_object_put(root);
        root=NULL;
    }
    json_tokener_free(tok);
    return root;
}
static const char *value(struct json_object *root,const char *key)
{
    struct json_object *v=NULL;
    if(root&&json_object_object_get_ex(root,key,&v) &&
       json_object_is_type(v,json_type_string))return json_object_get_string(v);
    return NULL;
}
static int integer(struct json_object *root,const char *key)
{
    struct json_object *v=NULL;
    if(root&&json_object_object_get_ex(root,key,&v) &&
       json_object_is_type(v,json_type_int))return json_object_get_int(v);
    return -1;
}
static void plain(char *out,size_t cap,const char *source)
{
    size_t n=0;
    if(!out||!cap)return;
    if(!source){out[0]=0;return;}
    while(*source&&n+1<cap){
        unsigned char c=(unsigned char)*source++;
        if(c>=32&&c<127)out[n++]=(char)c;
        else if(c=='\t'||c=='\n'||c=='\r')out[n++]=' ';
    }
    out[n]=0;
}
static int emit(char *out,size_t cap,size_t *used,const char *fmt,...)
{
    va_list args;int n;
    if(*used>=cap)return 0;
    va_start(args,fmt);
    n=vsnprintf(out+*used,cap-*used,fmt,args);
    va_end(args);
    if(n<0||(size_t)n>=cap-*used)return 0;
    *used+=(size_t)n;
    return 1;
}
static int list_items(const response_t *r,const char *kind,char *out,size_t cap,size_t *used)
{
    struct json_object *array=decode(r,json_type_array);
    size_t i,n;
    if(!array)return 0;
    n=json_object_array_length(array);
    if(!emit(out,cap,used,"%s (%zu returned, bounded first page):\n",kind,n)){
        json_object_put(array);return 0;
    }
    for(i=0;i<n&&i<8;++i){
        struct json_object *item=json_object_array_get_idx(array,i);
        const char *name=value(item,"name"),*path=value(item,"path");
        const char *title=value(item,"title"),*sha=value(item,"sha");
        const char *url=value(item,"html_url"),*type=value(item,"type");
        char detail[180],link[250],extra[80];
        plain(detail,sizeof detail,name?name:path?path:title?title:sha?sha:"unknown");
        if(sha&&!strcmp(kind,"Recent commit references")){
            struct json_object *commit=NULL,*author=NULL;
            const char *message=NULL,*date=NULL;
            if(json_object_object_get_ex(item,"commit",&commit) &&
               json_object_is_type(commit,json_type_object)){
                message=value(commit,"message");
                if(json_object_object_get_ex(commit,"author",&author) &&
                   json_object_is_type(author,json_type_object))
                    date=value(author,"date");
            }
            if(message){
                char message_clean[105],sha_clean[16],date_clean[24];
                plain(message_clean,sizeof message_clean,message);
                plain(sha_clean,sizeof sha_clean,sha);
                plain(date_clean,sizeof date_clean,date?date:"date not provided");
                snprintf(detail,sizeof detail,"%.12s: %.95s (%s)",
                         sha_clean,message_clean,date_clean);
            }
        }
        plain(link,sizeof link,url?url:"");
        plain(extra,sizeof extra,type?type:"");
        if(!emit(out,cap,used,"- %s%s%s%s%s\n",detail,
                 extra[0]?" [":"",extra,extra[0]?"]":"",
                 link[0]?" (URL below)":"")){
            json_object_put(array);return 0;
        }
        if(link[0] && !emit(out,cap,used,"  %s\n",link)){
            json_object_put(array);return 0;
        }
    }
    json_object_put(array);
    return 1;
}
static int repo_summary(const response_t *r,char *out,size_t cap,size_t *used)
{
    struct json_object *root=decode(r,json_type_object);
    const char *name,*desc,*branch,*url,*language;
    char clean[400];
    int stars,forks,issues;
    if(!root)return 0;
    name=value(root,"full_name");desc=value(root,"description");
    branch=value(root,"default_branch");url=value(root,"html_url");
    language=value(root,"language");
    if(!name||!url||!branch){json_object_put(root);return 0;}
    plain(clean,sizeof clean,desc?desc:"No description supplied");
    stars=integer(root,"stargazers_count");
    forks=integer(root,"forks_count");
    issues=integer(root,"open_issues_count");
    if(!emit(out,cap,used,
        "Repository: %s\nURL: %s\nDescription: %s\n"
        "Default branch: %s\nPrimary language: %s\n"
        "GitHub counts: stars %d, forks %d, open issues/PRs %d "
        "(GitHub open_issues_count includes pull requests).\n",
        name,url,clean,branch,language?language:"unreported",
        stars,forks,issues)){json_object_put(root);return 0;}
    json_object_put(root);
    return 1;
}
static int base64_digit(char c)
{
    if(c>='A'&&c<='Z')return c-'A';
    if(c>='a'&&c<='z')return c-'a'+26;
    if(c>='0'&&c<='9')return c-'0'+52;
    if(c=='+')return 62;
    if(c=='/')return 63;
    return -1;
}
static int file_excerpt(struct json_object *item,char *out,size_t cap,size_t *used)
{
    const char *content=value(item,"content");
    const char *encoding=value(item,"encoding");
    const char *size=value(item,"size");
    size_t i,n=0,seen=0;
    unsigned bits=0,acc=0;
    char excerpt[1300];
    (void)size;
    if(!content||!encoding||strcmp(encoding,"base64"))
        return emit(out,cap,used,"File contents unavailable as a readable Base64 source blob.\n");
    for(i=0;content[i]&&n+1<sizeof excerpt;++i){
        int digit;
        unsigned char c;
        if(content[i]=='='||content[i]=='\r'||content[i]=='\n')
            continue;
        digit=base64_digit(content[i]);
        if(digit<0)return 0;
        acc=(acc<<6)|(unsigned)digit;
        bits+=6;
        if(bits<8)continue;
        bits-=8;
        c=(unsigned char)((acc>>bits)&255U);
        ++seen;
        if(c==0)return emit(out,cap,used,
            "Binary content detected; source excerpt omitted.\n");
        if(c=='\n'||c=='\r')excerpt[n++]='\n';
        else if(c=='\t')excerpt[n++]=' ';
        else if(c>=32&&c<127)excerpt[n++]=(char)c;
    }
    excerpt[n]=0;
    return emit(out,cap,used,
        "File content excerpt (max 1299 plain-text bytes; inspect linked source for full context):\n%s\n",
        excerpt);
}
static int parse_query(const char *query,char owner[80],char repo[80],
                       char action[32],char path[320])
{
    char copy[512],*save=NULL,*name,*operation,*extra,*fourth;
    if(!query||strlen(query)>=sizeof copy)return 0;
    snprintf(copy,sizeof copy,"%s",query);
    name=strtok_r(copy," \t\r\n",&save);
    operation=strtok_r(NULL," \t\r\n",&save);
    extra=strtok_r(NULL," \t\r\n",&save);
    fourth=strtok_r(NULL," \t\r\n",&save);
    if(!repository(name,owner,repo)||fourth)return 0;
    if(!operation)operation="summary";
    if(strcmp(operation,"summary")&&strcmp(operation,"tree")&&
       strcmp(operation,"commits")&&strcmp(operation,"issues")&&
       strcmp(operation,"prs")&&strcmp(operation,"file"))return 0;
    if(!strcmp(operation,"file")||!strcmp(operation,"tree")){
        if(extra&& !path_allowed(extra))return 0;
        if(!strcmp(operation,"file")&&!extra)return 0;
    }else if(extra)return 0;
    snprintf(action,32,"%s",operation);
    snprintf(path,320,"%s",extra?extra:"");
    return 1;
}
static int run(const char *query,char *out,size_t cap)
{
    char owner[80],repo[80],action[32],path[320],endpoint[1000];
    response_t *response;
    size_t used=0;
    int result=2;
    if(!out||cap<300)return 3;
    out[0]=0;
    if(!parse_query(query,owner,repo,action,path)){
        snprintf(out,cap,"Usage: /github owner/repo [summary|tree [path]|file path|commits|issues|prs]. Read-only.");
        return 3;
    }
    if(!permit(owner,repo)){
        snprintf(out,cap,"Repository not authorized for Digit's GitHub read-only module.");
        return 1;
    }
    response=calloc(1,sizeof *response);
    if(!response)return 2;
    if(!strcmp(action,"summary"))snprintf(endpoint,sizeof endpoint,"%s/%s",owner,repo);
    else if(!strcmp(action,"tree"))
        snprintf(endpoint,sizeof endpoint,"%s/%s/contents%s%s",
                 owner,repo,*path?"/":"",path);
    else if(!strcmp(action,"file"))
        snprintf(endpoint,sizeof endpoint,"%s/%s/contents/%s",owner,repo,path);
    else if(!strcmp(action,"prs"))
        snprintf(endpoint,sizeof endpoint,"%s/%s/pulls?state=open&per_page=8",owner,repo);
    else snprintf(endpoint,sizeof endpoint,"%s/%s/%s?per_page=8",
                  owner,repo,action);
    if(!github_get(endpoint,response)){
        snprintf(out,cap,"GitHub data unavailable (network, permission, rate limit or missing resource). No report asserted.");
        goto done;
    }
    if(!emit(out,cap,&used,"Read-only GitHub evidence: %s/%s\nOperation: %s\n",
             owner,repo,action))goto done;
    if(!strcmp(action,"summary")){
        result=repo_summary(response,out,cap,&used)?0:2;
        if(result==0){
            const char *sections[]={"contents","commits?per_page=5",
                                     "issues?state=open&per_page=5",
                                     "pulls?state=open&per_page=5"};
            const char *labels[]={"Root entries","Recent commit references",
                                  "Open issues (may include PRs)","Open pull requests"};
            size_t section;
            for(section=0;section<4;++section){
                response_t *more=calloc(1,sizeof *more);
                if(!more)break;
                if(snprintf(endpoint,sizeof endpoint,"%s/%s/%s",
                            owner,repo,sections[section])<(int)sizeof endpoint &&
                   github_get(endpoint,more)){
                    if(!emit(out,cap,&used,"\n") ||
                       !list_items(more,labels[section],out,cap,&used)){
                        free(more);
                        break;
                    }
                }else if(!emit(out,cap,&used,
                              "\n%s: unavailable; no results asserted.\n",
                              labels[section])){
                    free(more);
                    break;
                }
                free(more);
            }
        }
    }
    else if(!strcmp(action,"tree"))result=list_items(response,"Entries",out,cap,&used)?0:2;
    else if(!strcmp(action,"commits")){
        /* GitHub commit records contain a nested commit.message; use SHA and URL as stable evidence. */
        result=list_items(response,"Recent commit references",out,cap,&used)?0:2;
    }else if(!strcmp(action,"issues")||!strcmp(action,"prs"))
        result=list_items(response,!strcmp(action,"issues")?"Open issues (may include PRs)":"Open pull requests",
                          out,cap,&used)?0:2;
    else {
        struct json_object *item=decode(response,json_type_object);
        const char *type=value(item,"type"),*sha=value(item,"sha");
        const char *url=value(item,"html_url");
        char clean[350];
        if(item&&type&&sha&&url&&!strcmp(type,"file")){
            plain(clean,sizeof clean,url);
            result=emit(out,cap,&used,
                "File: %s\nBlob SHA: %s\nSource: %s\n",
                path,sha,clean)&&file_excerpt(item,out,cap,&used)?0:2;
        }
        if(item)json_object_put(item);
    }
    if(result!=0)snprintf(out,cap,"GitHub response unavailable or malformed; no findings asserted.");
done:
    free(response);
    return result;
}
static stnlabz_module_result_t report_service(const void *request,size_t req_size,
    void *response,size_t capacity,size_t *used,void *ctx)
{
    const digit_github_request_t *in=request;
    digit_github_response_t *out=response;
    (void)ctx;
    if(used)*used=0;
    if(!in||req_size!=sizeof *in||!out||capacity<sizeof *out||
       !memchr(in->query,0,sizeof in->query))return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(out,0,sizeof *out);
    out->status=run(in->query,out->report,sizeof out->report);
    if(used)*used=sizeof *out;
    return STNLABZ_MODULE_OK;
}
/* Core's route capture accepts one reply, at most 511 bytes, per command.
 * Longer evidence is available through the report service; never emit a
 * second callback or turn a valid route into INVALID_STATE. */
static int command_reply_text(const char *report,char out[512])
{
    size_t i=0,n=0,length;
    const char *suffix=" [truncated; use /github ... tree, file, commits, issues or prs]";
    if(!report||!out||!report[0])return 0;
    length=strlen(report);
    while(i<length&&n<511U){
        char c=report[i++];
        if(c=='\n'||c=='\r'||c=='\t')c=' ';
        if(c==' ' && (!n||out[n-1]==' '))continue;
        out[n++]=c;
    }
    if(i<length){
        size_t trailing=strlen(suffix);
        if(trailing<n){
            n-=trailing;
            while(n&&out[n-1]==' ')--n;
            memcpy(out+n,suffix,trailing);
            n+=trailing;
        }
    }
    while(n&&out[n-1]==' ')--n;
    out[n]=0;
    return n>0;
}
static stnlabz_module_result_t command(const stnlabz_module_command_t *cmd,
    stnlabz_module_command_reply_fn reply,void *context,void *ctx)
{
    digit_github_response_t *result;
    char concise[512];
    int accepted;
    (void)ctx;
    if(!cmd||!reply||!memchr(cmd->arguments,0,sizeof cmd->arguments))
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    result=calloc(1,sizeof *result);
    if(!result)return STNLABZ_MODULE_ERR_INVALID_STATE;
    result->status=run(cmd->arguments,result->report,sizeof result->report);
    accepted=command_reply_text(result->report,concise) && reply(context,concise);
    free(result);
    return accepted?STNLABZ_MODULE_OK:STNLABZ_MODULE_ERR_INVALID_STATE;
}
static int qualification(unsigned *executed,unsigned *passed,int *negative)
{
    unsigned e=0,p=0;
    char owner[80],repo[80],action[32],path[320];
    response_t fixture={0};
    struct json_object *parsed;
    size_t used=0;
    char report[2048];
#define CHECK(x) do{++e;if(x)++p;}while(0)
    CHECK(repository("poemei/digit",owner,repo));
    CHECK(!strcmp(owner,"poemei")&&!strcmp(repo,"digit"));
    CHECK(!repository("poemei/digit/extra",owner,repo));
    CHECK(!repository("../digit",owner,repo));
    CHECK(!repository("owner/",owner,repo));
    CHECK(path_allowed("src/runner.c"));
    CHECK(!path_allowed("../secrets"));
    CHECK(!path_allowed("src/../../passwd"));
    CHECK(!path_allowed("/etc/passwd"));
    CHECK(parse_query("poemei/digit",owner,repo,action,path));
    CHECK(!strcmp(action,"summary"));
    CHECK(parse_query("poemei/digit file src/core.c",owner,repo,action,path));
    CHECK(!strcmp(action,"file")&&!strcmp(path,"src/core.c"));
    CHECK(!parse_query("poemei/digit push",owner,repo,action,path));
    CHECK(!parse_query("poemei/digit file",owner,repo,action,path));
    CHECK(!parse_query("poemei/digit tree ../../etc",owner,repo,action,path));
    CHECK(!parse_query("poemei/digit issues extra",owner,repo,action,path));
    snprintf(fixture.bytes,sizeof fixture.bytes,
        "{\"full_name\":\"poemei/digit\",\"html_url\":\"https://github.com/poemei/digit\","
        "\"default_branch\":\"main\",\"description\":\"Test repository\","
        "\"language\":\"C\",\"stargazers_count\":1,\"forks_count\":0,"
        "\"open_issues_count\":2}");
    fixture.size=strlen(fixture.bytes);
    CHECK(repo_summary(&fixture,report,sizeof report,&used));
    CHECK(strstr(report,"Test repository")!=NULL);
    parsed=decode(&fixture,json_type_object);
    CHECK(parsed!=NULL);
    if(parsed)json_object_put(parsed);
    CHECK(!decode(&fixture,json_type_array));
    {
        struct json_object *file=json_object_new_object();
        size_t used_file=0;
        char excerpt[512]={0};
        json_object_object_add(file,"content",json_object_new_string("SGVsbG8gZ2l0aHViIQo="));
        json_object_object_add(file,"encoding",json_object_new_string("base64"));
        CHECK(file_excerpt(file,excerpt,sizeof excerpt,&used_file));
        CHECK(strstr(excerpt,"Hello github!")!=NULL);
        CHECK(strstr(excerpt,"source blob")==NULL);
        json_object_put(file);
    }

    CHECK(!strcmp(DIGIT_GITHUB_REPORT_SERVICE,"digit.github.report.v1"));
    {
        char concise[512],long_text[1400];
        memset(long_text,'A',sizeof long_text-1U);
        long_text[sizeof long_text-1U]=0;
        CHECK(command_reply_text(long_text,concise));
        CHECK(strlen(concise)<512U);
        CHECK(strstr(concise,"[truncated;")!=NULL);
        CHECK(command_reply_text("one\\ntwo",concise));
        CHECK(!strcmp(concise,"one two"));
        CHECK(!command_reply_text("",concise));
    }
#undef CHECK
    if(executed)*executed=e;
    if(passed)*passed=p;
    if(negative)*negative=e>=10&&e==p;
    return e>=10&&e==p;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    unsigned executed=0,passed=0;int negative=0;
    if(!q)return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q,0,sizeof *q);
    if(!qualification(&executed,&passed,&negative))
        return STNLABZ_MODULE_ERR_QUALIFICATION;
    q->tests_executed=executed;q->tests_passed=passed;
    q->negative_test_executed=1;q->negative_test_passed=negative;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *host)
{
    if(!host||!host->register_command||!host->unregister_command||
       !host->register_service||!host->unregister_service)
        return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if(!load_allowlist(GITHUB_ALLOWLIST_PATH))return STNLABZ_MODULE_ERR_START_FAILED;
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)return STNLABZ_MODULE_ERR_START_FAILED;
    host_ref=host;
    if(!host->register_service(DIGIT_GITHUB_REPORT_SERVICE,report_service,NULL))goto fail;
    service_registered=1;
    if(!host->register_command("github",command,NULL))goto fail;
    command_registered=1;
    return STNLABZ_MODULE_OK;
fail:
    if(service_registered)host->unregister_service(DIGIT_GITHUB_REPORT_SERVICE,NULL);
    service_registered=0;host_ref=NULL;allowed_count=0;
    curl_global_cleanup();
    return STNLABZ_MODULE_ERR_START_FAILED;
}
static stnlabz_module_result_t stop(void)
{
    if(command_registered&&host_ref&&
       !host_ref->unregister_command("github",NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    command_registered=0;
    if(service_registered&&host_ref&&
       !host_ref->unregister_service(DIGIT_GITHUB_REPORT_SERVICE,NULL))
        return STNLABZ_MODULE_ERR_STOP_FAILED;
    service_registered=0;host_ref=NULL;allowed_count=0;
    curl_global_cleanup();
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor={
    "github","Digit GitHub Read-Only",1,0,1,
    STNLABZ_MODULE_API_MAJOR,STNLABZ_MODULE_API_MINOR,
    qualify,start,stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{return &descriptor;}
