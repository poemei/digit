#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <json-c/json.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "s2_model.h"

static uint64_t hash_bytes(const char *s, size_t length)
{
    size_t i;
    uint64_t h = UINT64_C(14695981039346656037);
    for (i = 0; i < length; ++i) {
        h ^= (unsigned char)s[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}
static int valid_id(const char *s)
{
    size_t n;
    if (!s || strncmp(s, "t_", 2)) return 0;
    for (n = 0; s[n]; ++n)
        if (n >= 95U || !(isalnum((unsigned char)s[n]) ||
              s[n] == '_' || s[n] == '.' || s[n] == '-')) return 0;
    return n > 2U;
}
static int reserve(s2_register_t *r)
{
    s2_case_t *next;
    size_t size;
    if (r->count < r->capacity) return 1;
    if (r->capacity > SIZE_MAX / 2U) return 0;
    size = r->capacity ? r->capacity * 2U : 1024U;
    if (size > SIZE_MAX / sizeof *next) return 0;
    next = realloc(r->cases, size * sizeof *next);
    if (!next) return 0;
    r->cases = next; r->capacity = size;
    return 1;
}
static s2_case_t *find_case(s2_register_t *r, const char *id)
{
    size_t i;
    for (i = 0; i < r->count; ++i)
        if (!strcmp(r->cases[i].id, id)) return &r->cases[i];
    return NULL;
}
static const char *jstring(struct json_object *item, const char *key)
{
    struct json_object *value = NULL;
    if (!json_object_object_get_ex(item, key, &value) ||
        !json_object_is_type(value, json_type_string)) return NULL;
    return json_object_get_string(value);
}
static int severity_qualifies(struct json_object *item)
{
    const char *s = jstring(item, "severity");
    struct json_object *details = NULL;
    if (!s && json_object_object_get_ex(item, "details", &details) &&
        json_object_is_type(details, json_type_object))
        s = jstring(details, "severity");
    return s && (!strcmp(s, "MEDIUM") || !strcmp(s, "HIGH") ||
                 !strcmp(s, "CRITICAL"));
}
int s2_ingest_json(s2_register_t *state, const char *body, size_t bytes)
{
    struct json_tokener *tokener = NULL;
    struct json_object *root = NULL, *threats = NULL;
    enum json_tokener_error err;
    s2_register_t incoming = {0};
    size_t i, n;
    int ok = 0;
    if (!state || !body || !bytes || bytes > 8U * 1024U * 1024U ||
        bytes > INT32_MAX) return 0;
    tokener = json_tokener_new();
    if (!tokener) return 0;
    root = json_tokener_parse_ex(tokener, body, (int)bytes);
    err = json_tokener_get_error(tokener);
    if (err != json_tokener_success || !root ||
        !json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "threats", &threats) ||
        !json_object_is_type(threats, json_type_array)) goto done;
    n = json_object_array_length(threats);
    if (n > 10000U) goto done;
    for (i = 0; i < n; ++i) {
        struct json_object *row = json_object_array_get_idx(threats, i);
        const char *id, *encoded;
        s2_case_t *existing;
        uint64_t digest;
        if (!row || !json_object_is_type(row, json_type_object) ||
            !(id = jstring(row, "id")) || !valid_id(id)) {
            incoming.invalid++;
            continue;
        }
        /* JSON object is canonicalized by json-c for this ingestion.
           A change in retained source fields changes the signature. */
        encoded = json_object_to_json_string_ext(row, JSON_C_TO_STRING_PLAIN);
        if (!encoded) goto done;
        digest = hash_bytes(encoded, strlen(encoded));
        if (find_case(&incoming, id)) {
            incoming.invalid++;
            continue;
        }
        if (!reserve(&incoming)) goto done;
        existing = &incoming.cases[incoming.count++];
        (void)snprintf(existing->id, sizeof existing->id, "%s", id);
        existing->hash = digest;
        if (severity_qualifies(row)) incoming.qualifying++;
        else if (!jstring(row, "severity")) incoming.missing_severity++;
    }
    /* Malformed or conflicting evidence must not overwrite known cases. */
    if (incoming.invalid) goto done;
    for (i = 0; i < incoming.count; ++i) {
        s2_case_t *previous = find_case(state, incoming.cases[i].id);
        if (!previous) incoming.opened++;
        else if (previous->hash != incoming.cases[i].hash) incoming.changed++;
        else incoming.unchanged++;
    }
    /* Retain records absent from the current snapshot; absence is not closure. */
    for (i = 0; i < state->count; ++i)
        if (!find_case(&incoming, state->cases[i].id)) {
            if (!reserve(&incoming)) goto done;
            incoming.cases[incoming.count++] = state->cases[i];
        }
    free(state->cases);
    *state = incoming;
    memset(&incoming, 0, sizeof incoming);
    ok = 1;
done:
    s2_register_free(&incoming);
    if (root) json_object_put(root);
    json_tokener_free(tokener);
    return ok;
}
void s2_register_free(s2_register_t *state)
{
    if (!state) return;
    free(state->cases);
    memset(state, 0, sizeof *state);
}
int s2_load_register(s2_register_t *state, const char *path)
{
    char line[256], id[96], tail;
    uint64_t hash;
    FILE *f;
    if (!state || !path) return 0;
    f = fopen(path, "r");
    if (!f) return errno == ENOENT;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%95s %" SCNx64 " %c", id, &hash, &tail) != 2 ||
            !valid_id(id) || find_case(state, id) || !reserve(state)) {
            (void)fclose(f); return 0;
        }
        (void)snprintf(state->cases[state->count].id,
                       sizeof state->cases[state->count].id, "%s", id);
        state->cases[state->count++].hash = hash;
    }
    if (ferror(f)) { (void)fclose(f); return 0; }
    return fclose(f) == 0;
}
int s2_save_register(const s2_register_t *state, const char *path)
{
    char temp[1024];
    FILE *f;
    size_t i;
    int fd;
    if (!state || !path || snprintf(temp, sizeof temp, "%s.tmp.XXXXXX", path) >=
        (int)sizeof temp) return 0;
    fd = mkstemp(temp);
    if (fd < 0) return 0;
    f = fdopen(fd, "w");
    if (!f) { (void)close(fd); (void)unlink(temp); return 0; }
    for (i = 0; i < state->count; ++i)
        if (fprintf(f, "%s %016" PRIx64 "\n", state->cases[i].id,
                    state->cases[i].hash) < 0) break;
    if (i != state->count || fflush(f) || fsync(fd) || fclose(f)) {
        (void)unlink(temp); return 0;
    }
    if (rename(temp, path)) { (void)unlink(temp); return 0; }
    return 1;
}
static int verify(int condition, unsigned *e, unsigned *p)
{
    ++*e; if (condition) ++*p; return condition;
}
int s2_model_selftest(unsigned *executed, unsigned *passed,
                      int *negative_executed, int *negative_passed)
{
    const char *feed1 = "{\"threats\":[{\"id\":\"t_one\",\"severity\":\"MEDIUM\",\"ip\":\"1.1.1.1\"}]}";
    const char *feed2 = "{\"threats\":[{\"id\":\"t_one\",\"severity\":\"HIGH\",\"ip\":\"1.1.1.1\"},{\"id\":\"t_two\",\"type\":\"pattern_match\",\"details\":{\"ip_address\":\"2.2.2.2\"}}]}";
    s2_register_t r = {0};
    unsigned e = 0, p = 0;
    int ne = 0, np = 0;
    verify(s2_ingest_json(&r, feed1, strlen(feed1)), &e, &p);
    verify(r.count == 1U, &e, &p);
    verify(r.opened == 1U, &e, &p);
    verify(r.qualifying == 1U, &e, &p);
    verify(s2_ingest_json(&r, feed1, strlen(feed1)), &e, &p);
    verify(r.opened == 0U && r.unchanged == 1U, &e, &p);
    verify(s2_ingest_json(&r, feed2, strlen(feed2)), &e, &p);
    verify(r.count == 2U && r.opened == 1U && r.changed == 1U, &e, &p);
    verify(r.missing_severity == 1U, &e, &p);
    verify(r.qualifying == 1U, &e, &p);
    ne = 1;
    np = !s2_ingest_json(&r, "{\"threats\":{\"id\":\"t_bad\"}}", 32U) &&
         r.count == 2U;
    s2_register_free(&r);
    if (executed) *executed = e;
    if (passed) *passed = p;
    if (negative_executed) *negative_executed = ne;
    if (negative_passed) *negative_passed = np;
    return e == 10U && p == 10U && ne && np;
}
