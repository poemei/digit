#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "stnlabz_abi/module.h"
#include "s2_model.h"
#include "s2_public.h"
#include "digit_llm.h"

#define S2_URL "https://api.stn-labz.com/threats"
#define S2_MAX_BYTES (8U * 1024U * 1024U)
#define S2_STATE_DIR "/opt/digit/state/s2"
#define S2_STATE_FILE S2_STATE_DIR "/sentinel-register.txt"
#define S2_POLL_SECONDS 3600U
#define S2_PUBLISH_SECONDS 3600U
#define S2_PUBLIC_STAMP S2_STATE_DIR "/last-published"
typedef struct { char *body; size_t length; } received_t;
static pthread_t worker;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int stop_requested;
static int thread_running, registered;
static const stnlabz_module_host_t *owner;
static s2_register_t cases;
static char last_result[400] = "No successful Sentinel collection yet.";
static size_t latest_opened, latest_changed, latest_qualifying;
static time_t last_success;
static int initial_report_sent;
static char latest_public_report[8192];
static time_t last_publish_attempt;
static void add_reader_note(char *report, size_t capacity);

static size_t on_bytes(char *data, size_t size, size_t amount, void *ctx)
{
    received_t *r = ctx;
    char *next;
    size_t n;
    if (!r || !data || !size || amount > SIZE_MAX / size) return 0;
    n = size * amount;
    if (n > S2_MAX_BYTES - r->length) return 0;
    next = realloc(r->body, r->length + n + 1U);
    if (!next) return 0;
    r->body = next;
    memcpy(r->body + r->length, data, n);
    r->length += n;
    r->body[r->length] = 0;
    return n;
}
static int collect_once(void)
{
    CURL *curl = curl_easy_init();
    received_t data = {0};
    CURLcode result;
    long status = 0;
    s2_register_t next = {0};
    int ok = 0;
    int announce = 0, baseline = 0;
    size_t report_total = 0, report_opened = 0, report_changed = 0;
    if (!curl) return 0;
    curl_easy_setopt(curl, CURLOPT_URL, S2_URL);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_bytes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Digit-S2/1.0.3");
    result = curl_easy_perform(curl);
    if (result == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status != 200L || !data.body) goto done;
    pthread_mutex_lock(&mutex);
    if (cases.count) {
        next.cases = malloc(cases.count * sizeof *next.cases);
        if (!next.cases) {
            pthread_mutex_unlock(&mutex);
            goto done;
        }
        memcpy(next.cases, cases.cases, cases.count * sizeof *next.cases);
        next.count = cases.count;
        next.capacity = cases.count;
    }
    pthread_mutex_unlock(&mutex);
    if (!s2_ingest_json(&next, data.body, data.length)) goto done;
    if (!s2_save_register(&next, S2_STATE_FILE)) goto done;
    pthread_mutex_lock(&mutex);
    s2_register_free(&cases);
    cases = next;
    memset(&next, 0, sizeof next);
    latest_opened = cases.opened;
    latest_changed = cases.changed;
    latest_qualifying = cases.qualifying;
    last_success = time(NULL);
    baseline = !initial_report_sent;
    announce = baseline || cases.opened != 0U || cases.changed != 0U;
    report_total = cases.count;
    report_opened = cases.opened;
    report_changed = cases.changed;
    if (announce) initial_report_sent = 1;
    (void)snprintf(last_result, sizeof last_result,
        "Sentinel source records tracked=%zu; this poll new=%zu revised=%zu unchanged=%zu; "
        "MEDIUM/HIGH/CRITICAL observed=%zu; records without explicit severity=%zu. "
        "Source findings are investigative leads, not proof of compromise.",
        cases.count, cases.opened, cases.changed, cases.unchanged,
        cases.qualifying, cases.missing_severity);
    pthread_mutex_unlock(&mutex);
    fprintf(stderr, "[S2] %s\n", last_result);
    ok = 1;
done:
    if (!ok) {
        /* Failure does not rewrite the register, reset historical cases,
           or misreport an unsuccessful poll as a clean snapshot. */
        fprintf(stderr, "[S2] Sentinel collection failed curl=%d http=%ld\n",
                (int)result, status);
    }
    if (ok) {
        char prepared[sizeof latest_public_report];
        if (s2_public_render(data.body, data.length, report_total, report_opened,
                             report_changed, time(NULL), prepared, sizeof prepared)) {
            add_reader_note(prepared,sizeof prepared);
            (void)snprintf(latest_public_report, sizeof latest_public_report, "%s", prepared);
        } else {
            latest_public_report[0] = 0;
            fprintf(stderr,"[S2] public report withheld: validation failed\n");
        }
    }
    s2_register_free(&next);
    free(data.body);
    return ok;
}
/* Ollama is an optional language editor for general reader education only.
 * It sees no raw Sentinel evidence, customer data, IPs or internal details.
 * S2 retains final publication authority and rejects unsupported framing. */
static void add_reader_note(char *report,size_t capacity)
{
 digit_llm_request_t request={{0}};
 digit_llm_response_t response={{0}};
 size_t used=0U,i,n;
 char *marker;
 const char *heading="## A QUICK EXPLANATION FROM DIGIT\n\n";
 if(!owner||!owner->invoke_service||!report)return;
 (void)snprintf(request.prompt,sizeof request.prompt,
  "Write one friendly, general educational sentence for a nontechnical small-business reader. "
  "Explain why bots look for common website login or configuration pages. "
  "Do not discuss any specific case, investigation, IP, organization, access, success, breach, money or damage. "
  "Do not add numbers, quotes, names, links, Markdown or claims about what was observed. "
  "One sentence under 180 characters, plain English only.");
 if(owner->invoke_service(DIGIT_LLM_GENERATE_SERVICE,&request,sizeof request,
      &response,sizeof response,&used)!=STNLABZ_MODULE_OK||
    used!=sizeof response||!memchr(response.text,0,sizeof response.text))return;
 n=strlen(response.text);
 if(n<30U||n>180U)return;
 for(i=0;i<n;++i){
  unsigned char c=(unsigned char)response.text[i];
  if(c<32||c>126||c=='#'||c=='<'||c=='>'||c=='['||c==']'||
     c=='{'||c=='}'||c=='/'||c=='\\'||(c>='0'&&c<='9'))return;
 }
 /* This deliberately rejects incident-specific words. An untrusted model
    cannot author evidence or business-impact conclusions for publication. */
 {
  static const char *blocked[]={
   "breach","hacked","stolen","compromis","attacker","exploited",
   "sentinel","rictus","digit","stn","confirmed","detected","found",
   "we saw","we investigated","our servers","successful",NULL
  };
  char lower[200];
  for(i=0;i<n;++i)lower[i]=(char)tolower((unsigned char)response.text[i]);
  lower[n]=0;
  for(i=0;blocked[i];++i)if(strstr(lower,blocked[i]))return;
 }
 marker=strstr(report,"## 9. REPORT CONCLUSION");
 if(marker){
  char insert[280];
  size_t pos=(size_t)(marker-report),length=strlen(report),added;
  int w=snprintf(insert,sizeof insert,"%s%s\n\n",heading,response.text);
  if(w<=0||(size_t)w>=sizeof insert)return;
  added=(size_t)w;
  if(length+added>=capacity)return;
  memmove(report+pos+added,report+pos,length-pos+1U);
  memcpy(report+pos,insert,added);
 }
}
static time_t publish_stamp(void)
{
 struct stat st;
 if(stat(S2_PUBLIC_STAMP,&st)==0&&S_ISREG(st.st_mode))return st.st_mtime;
 return (time_t)0;
}
static int record_publish_stamp(void)
{
 FILE *f=fopen(S2_PUBLIC_STAMP,"w");
 if(!f)return 0;
 if(fprintf(f,"S2 sai-dsa published successfully\n")<0){fclose(f);return 0;}
 return fclose(f)==0;
}
static void *run_collection(void *unused)
{
 unsigned poll_wait=0U;
 time_t last_published=publish_stamp();
 (void)unused;
 while(!atomic_load(&stop_requested)){
  time_t now=time(NULL);
  if(poll_wait==0U){
   (void)collect_once();
   poll_wait=S2_POLL_SECONDS;
  }
  if(latest_public_report[0]&&now>0&&
     (last_published==0||(now>=last_published&&
      (unsigned long)(now-last_published)>=S2_PUBLISH_SECONDS))&&
     (last_publish_attempt==0||now-last_publish_attempt>=3600)){
   last_publish_attempt=now;
   if(s2_public_publish(latest_public_report)){
    if(record_publish_stamp()){
     last_published=now;
     fprintf(stderr,"[S2] sai-dsa published; hourly testing cadence active\n");
     if(owner&&owner->send_message){
      if(!owner->send_message("[S2] DSA has been updated: https://stn-labz.com/sai-dsa"))
       fputs("[S2] IRC DSA update notification failed\n",stderr);
     }
    }else fprintf(stderr,"[S2] publication accepted but stamp persistence failed\n");
   }else fprintf(stderr,"[S2] sai-dsa publishing failed or denied; previous report preserved\n");
  }
  {
   struct timespec wait={1,0};
   (void)nanosleep(&wait,NULL);
  }
  if(poll_wait>0U)poll_wait--;
 }
 return NULL;
}
static stnlabz_module_result_t s2_command(
    const stnlabz_module_command_t *cmd,
    stnlabz_module_command_reply_fn reply, void *reply_context, void *ctx)
{
    char output[480];
    char collected_at[48] = "never";
    struct tm utc;
    (void)ctx;
    if (!cmd || !reply) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (cmd->arguments[0] && strcmp(cmd->arguments, "status"))
        return reply(reply_context, "S2 supports /s2 and /s2 status.")
            ? STNLABZ_MODULE_OK : STNLABZ_MODULE_ERR_INVALID_STATE;
    pthread_mutex_lock(&mutex);
    if (last_success > (time_t)0 && gmtime_r(&last_success, &utc) &&
        strftime(collected_at, sizeof collected_at,
                 "%Y-%m-%d %H:%M:%S UTC", &utc) == 0U)
        (void)snprintf(collected_at, sizeof collected_at, "unavailable");
    (void)snprintf(output, sizeof output, "[S2] %s Last successful collection: %s. "
        "Case counts measure source records, not attacks.",
        last_result, collected_at);
    pthread_mutex_unlock(&mutex);
    return reply(reply_context, output) ? STNLABZ_MODULE_OK :
        STNLABZ_MODULE_ERR_INVALID_STATE;
}
static stnlabz_module_result_t qualify(stnlabz_module_qualification_result_t *q)
{
    unsigned e = 0, p = 0; int ne = 0, np = 0;
    if (!q) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    memset(q, 0, sizeof *q);
    if (!s2_model_selftest(&e, &p, &ne, &np)) return STNLABZ_MODULE_ERR_QUALIFICATION;
    q->tests_executed=e; q->tests_passed=p; q->tests_failed=e-p;
    q->negative_test_executed=ne; q->negative_test_passed=np;
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t start(const stnlabz_module_host_t *h)
{
    if (!h || !h->register_command || !h->unregister_command) return STNLABZ_MODULE_ERR_INVALID_ARGUMENT;
    if (mkdir("/opt/digit/state", 0700) && errno != EEXIST)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (mkdir(S2_STATE_DIR, 0700) && errno != EEXIST)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (!s2_load_register(&cases, S2_STATE_FILE))
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        return STNLABZ_MODULE_ERR_START_FAILED;
    if (!h->register_command("s2", s2_command, NULL)) {
        curl_global_cleanup();
        return STNLABZ_MODULE_ERR_START_FAILED;
    }
    owner = h; registered = 1;
    initial_report_sent = 0;
    latest_public_report[0] = 0;
    last_publish_attempt = 0;
    atomic_store(&stop_requested, 0);
    if (pthread_create(&worker, NULL, run_collection, NULL)) {
        (void)h->unregister_command("s2", NULL);
        registered = 0; owner = NULL;
        curl_global_cleanup();
        return STNLABZ_MODULE_ERR_START_FAILED;
    }
    thread_running = 1;
    fprintf(stderr, "[S2] module active, collection every %u seconds\n", S2_POLL_SECONDS);
    return STNLABZ_MODULE_OK;
}
static stnlabz_module_result_t stop(void)
{
    atomic_store(&stop_requested, 1);
    if (thread_running) {
        (void)pthread_join(worker, NULL);
        thread_running = 0;
    }
    if (registered && owner) (void)owner->unregister_command("s2", NULL);
    registered = 0; owner = NULL;
    pthread_mutex_lock(&mutex);
    s2_register_free(&cases);
    pthread_mutex_unlock(&mutex);
    curl_global_cleanup();
    return STNLABZ_MODULE_OK;
}
static const stnlabz_module_descriptor_t descriptor = {
    "s2", "Digit S2 Security Intelligence", 1, 2, 6,
    STNLABZ_MODULE_API_MAJOR, STNLABZ_MODULE_API_MINOR,
    qualify, start, stop
};
const stnlabz_module_descriptor_t *stnlabz_module_get_descriptor(void)
{
    return &descriptor;
}
