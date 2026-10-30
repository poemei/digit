#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "s2_model.h"
#include "s2_public.h"
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
        char report[8192];
        int prepared=s2_public_render(private_source,strlen(private_source),
                                      42U,0U,0U,time(NULL),report,sizeof report);
        if (!prepared || !strstr(report,"## BUSINESS DECISION") ||
            !strstr(report,"## SUPPORTING EVIDENCE") ||
            !strstr(report,"## BUSINESS / OPERATIONAL EFFECTS") ||
            !strstr(report,"HIGH: 1.") ||
            !strstr(report,"MEDIUM = OBSERVE") ||
            !strstr(report,"HIGH = ASSESS / CRITICAL = INVESTIGATE") ||
            !strstr(report,"## REMAINING INVESTIGATION") ||
            !strstr(report,"**Decision:**") ||
            strstr(report,"10.44.55.66") ||
            strstr(report,"internal.example") ||
            strstr(report,"SECRET-KEY-123")) ok=0;
        if(s2_public_render("not-json",8U,42U,0U,0U,time(NULL),
                            report,sizeof report))ok=0;
        if(s2_public_render(private_source,strlen(private_source),
                            42U,0U,0U,time(NULL),report,8U))ok=0;
    }
    (void)unlink(name);
    s2_register_free(&initial);
    s2_register_free(&restored);
    puts(ok ? "S2 persistence integration PASS" : "S2 persistence integration FAIL");
    return ok ? 0 : 1;
}
