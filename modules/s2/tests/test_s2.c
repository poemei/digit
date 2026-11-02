#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "s2_model.h"
#include "s2_public.h"
#include "s2_investigation.h"
#include <time.h>
int main(void)
{
    s2_register_t initial = {0}, restored = {0};
    unsigned executed = 0, passed = 0;
    int negative_executed = 0, negative_passed = 0;
    char name[] = "/tmp/digit-s2-test-XXXXXX";
    int fd;
    int ok = s2_model_selftest(&executed, &passed,
                               &negative_executed, &negative_passed);
    printf("S2 module qualification executed=%u passed=%u negative=%d/%d\n",
        executed, passed, negative_passed, negative_executed);
    if (!ok) return 1;
    fd = mkstemp(name);
    if (fd < 0) return 1;
    (void)close(fd);
    (void)unlink(name);
    if (!s2_ingest_json(&initial,
            "{\"threats\":[{\"id\":\"t_saved\",\"severity\":\"MEDIUM\"}]}",
            strlen("{\"threats\":[{\"id\":\"t_saved\",\"severity\":\"MEDIUM\"}]}")) ||
        !s2_save_register(&initial, name) ||
        !s2_load_register(&restored, name) ||
        restored.count != 1U ||
        strcmp(restored.cases[0].id, "t_saved")) ok = 0;
    {
        const char *private_source =
            "{\"threats\":[{\"id\":\"t_secret\",\"severity\":\"HIGH\","
            "\"ip\":\"10.44.55.66\",\"domain\":\"internal.example\","
            "\"response_status\":403,\"details\":{\"token\":\"SECRET-KEY-123\"}}]}";
        char report[32768];
        int prepared=s2_public_render(private_source,strlen(private_source),
                                      42U,0U,0U,time(NULL),report,sizeof report);
        if (!prepared || !strstr(report,"## DECISION") ||
            !strstr(report,"## WHAT DIGIT VERIFIED FROM AVAILABLE RECORDS") ||
            !strstr(report,"## BUSINESS / OPERATIONAL EFFECTS") ||
            !strstr(report,"1 HIGH") ||
            !strstr(report,"MEDIUM — OBSERVE") ||
            !strstr(report,"HIGH — ASSESS / CRITICAL — INVESTIGATE") ||
            !strstr(report,"## INDIVIDUAL HIGH / CRITICAL INVESTIGATIONS") ||
            !strstr(report,"**Business decision:**") ||
            strstr(report,"10.44.55.66") ||
            strstr(report,"internal.example") ||
            strstr(report,"SECRET-KEY-123")) ok=0;
        if (!strstr(report,"**Investigation 1 — HIGH**") ||
            !strstr(report,"access was denied") &&
            !strstr(report,"access was denied")) ok=0;
        {
            const char *nested="{\"threats\":[{\"id\":\"t_6ae374ed1453f\","
                "\"severity\":\"MEDIUM\",\"created_at\":\"2026-10-29 16:31:41\","
                "\"details\":{\"request_url\":\"/wp-admin/\",\"response_status\":403}},"
                "{\"id\":\"t_high_case\",\"severity\":\"HIGH\","
                "\"details\":{\"request_url\":\"/config.php\",\"response_status\":403}}]}";
            if(!s2_public_render(nested,strlen(nested),2U,0U,0U,
                                 time(NULL),report,sizeof report)||
               !strstr(report,"1 CMS admin/login")||
               !strstr(report,"1 configuration/test-file")||
               !strstr(report,"2 reported 403/429")||
               !strstr(report,"**Investigation 1 — HIGH**")||
               !strstr(report,"access was denied")||
               strstr(report,"t_6ae374ed1453f")||
               strstr(report,"/wp-admin/"))ok=0;
        }
        if(s2_public_render("not-json",8U,42U,0U,0U,time(NULL),
                            report,sizeof report))ok=0;
        if(s2_public_render(private_source,strlen(private_source),
                            42U,0U,0U,time(NULL),report,8U))ok=0;
    }
    {
        char journal[] = "/tmp/s2-history-test-XXXXXX";
        char detail[8192];
        s2_register_t tracked={0};
        const char *first="{\"threats\":[{\"id\":\"t_first\",\"severity\":\"HIGH\",\"response_status\":403}]}";
        const char *second="{\"threats\":[{\"id\":\"t_first\",\"severity\":\"CRITICAL\",\"response_status\":403}]}";
        int fd_history=mkstemp(journal), history_ok=fd_history>=0;
        if(history_ok){
            close(fd_history);
            unlink(journal);
            history_ok=s2_ingest_json(&tracked,first,strlen(first)) &&
                 s2_investigation_update(first,strlen(first),&tracked,journal,
                    (time_t)1700000000,detail,sizeof detail) &&
                 strstr(detail,"first recorded by this journal: 1")!=NULL;
        }
        if(history_ok){
            history_ok=s2_ingest_json(&tracked,second,strlen(second)) &&
                 s2_investigation_update(second,strlen(second),&tracked,journal,
                    (time_t)1700003600,detail,sizeof detail) &&
                 strstr(detail,"previously recorded and observed again: 1")!=NULL &&
                 strstr(detail,"Revised retained source records: 1")!=NULL &&
                 strstr(detail,"increased severity classifications: 1")!=NULL;
        }
        if(history_ok){
            history_ok=!s2_investigation_update("invalid",7,&tracked,journal,
                          (time_t)1700007200,detail,sizeof detail) &&
                 s2_investigation_update(second,strlen(second),&tracked,journal,
                    (time_t)1700003600,detail,sizeof detail) &&
                 strstr(detail,"Revised retained source records: 0")!=NULL;
        }
        if(history_ok){
            const char *pattern=
                "{\"threats\":["
                "{\"id\":\"t_site_a\",\"severity\":\"HIGH\","
                "\"ip\":\"198.51.100.10\",\"domain\":\"site-a.test\","
                "\"created_at\":\"2026-10-29 12:00:00\","
                "\"response_status\":403},"
                "{\"id\":\"t_site_b\",\"severity\":\"CRITICAL\","
                "\"ip\":\"198.51.100.10\",\"domain\":\"site-b.test\","
                "\"created_at\":\"2026-10-30 12:00:00\","
                "\"response_status\":429}]}";
            history_ok=s2_ingest_json(&tracked,pattern,strlen(pattern)) &&
                 s2_investigation_update(pattern,strlen(pattern),&tracked,journal,
                    (time_t)1700007200,detail,sizeof detail) &&
                 strstr(detail,"Indicator group 1")!=NULL &&
                 strstr(detail,"multiple reported site domains")!=NULL &&
                 strstr(detail,"across different reported source dates")!=NULL &&
                 strstr(detail,"2 reported denials or rate limits")!=NULL &&
                 strstr(detail,"198.51.100.10")==NULL &&
                 strstr(detail,"site-a.test")==NULL;
        }
        unlink(journal);
        s2_register_free(&tracked);
        if(!history_ok)ok=0;
        printf("S2 cross-collection investigation: %s\n",history_ok?"PASS":"FAIL");
    }
    (void)unlink(name);
    s2_register_free(&initial);
    s2_register_free(&restored);
    puts(ok ? "S2 persistence integration PASS" : "S2 persistence integration FAIL");
    return ok ? 0 : 1;
}
