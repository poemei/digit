/* [AI:GPT-6 | 2026-10-18] One integration check: dual-stream log routing.
 * Writes only to a temporary directory, never /opt/digit.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logging.h"

static int contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char line[2048];
    int found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, needle)) { found = 1; break; }
    fclose(f);
    return found;
}

int main(void)
{
    char directory[] = "/tmp/digit-logging-test-XXXXXX";
    char core[512], module[512];
    int ok;
    if (!mkdtemp(directory)) return 1;
    snprintf(core, sizeof core, "%s/digit.log", directory);
    snprintf(module, sizeof module, "%s/modules.log", directory);
    ok = stnlabz_log_open_path(core) &&
         stnlabz_log_is_open() &&
         stnlabz_log_event("CORE", "TEST_ROUTING", "check=core") &&
         stnlabz_log_event("MODULE", "TEST_ROUTING", "module=irc check=module") &&
         stnlabz_log_sync();
    stnlabz_log_close();
    ok = ok &&
         contains(core, "CORE TEST_ROUTING check=core") &&
         contains(module, "MODULE TEST_ROUTING") &&
         contains(module, "module=irc") &&
         !contains(core, "check=module") &&
         !contains(module, "check=core");
    unlink(core);
    unlink(module);
    rmdir(directory);
    if (!ok) {
        fputs("Logging dual-stream routing check FAILED\n", stderr);
        return 1;
    }
    puts("Logging dual-stream routing check passed (1 check)");
    return 0;
}
/* [End AI:GPT-6] */
